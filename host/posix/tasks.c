#ifdef PARATOS_IO_TEST
#include <pthread.h>
#endif
#include <sched.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <sys/mman.h>
#include <sys/wait.h>
#ifdef __linux__
#include <sys/syscall.h>
#endif

#include "gemdos.h"
#include "host.h"
#include "runtime.h"
#include "tasks.h"
#include "guest_memory.h"
#include "../../guest_exec.h"

#ifdef PARATOS_IO_TEST
static pthread_mutex_t cpu_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t terminal_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static _Thread_local sigjmp_buf fault_return;
static _Thread_local int finished, exit_status, cpu_locked;
static int16_t guest_pid, guest_parent_pid, runtime_root_id;

/* A shared record outlives the guest's CPU as long as a descendant can
 * still reference its image. Host wait ownership stays in the local list. */
enum { PID_SLOTS = 32768 };
typedef struct stage {
	uint32_t bp, env;
	uint16_t next, parent, dependents, status;
	int done, retained;
} stage;

typedef struct stage_family {
	stage entries[PID_SLOTS]; /* slot 0 is the initial guest */
	unsigned char reserved[PID_SLOTS];
	unsigned running;
} stage_family;

typedef struct stage_child {
	struct stage_child *next;
	pid_t host;
	int16_t id;
	int waitable, consumed;
} stage_child;

static stage_family *family;
static stage_child *children;
static uint16_t self_slot;
static int family_fd = -1;

#ifndef PARATOS_IO_TEST
static void record_lock(int byte, int type)
{
	struct flock lock = { .l_type = type, .l_whence = SEEK_SET, .l_start = byte, .l_len = 1 };
	int saved = errno;
	int result;
	do { result = fcntl(family_fd, F_SETLKW, &lock); } while (result < 0 && errno == EINTR);
	if (result < 0) { perror("ParaTOS guest lock"); _exit(1); }
	errno = saved;
}
#endif

static void cpu_lock(int type)
{
#ifndef PARATOS_IO_TEST
	if (family_fd >= 0 && PosixGuestMemoryShared()) record_lock(0, type);
#else
	if (type == F_UNLCK) pthread_mutex_unlock(&cpu_mutex);
	else pthread_mutex_lock(&cpu_mutex);
#endif
}

void PosixPause(void)
{
	cpu_locked = 0;
	cpu_lock(F_UNLCK);
}

void PosixResume(void)
{
	cpu_lock(F_WRLCK);
	cpu_locked = 1;
}

static void host_output(int fd, const char *buffer, size_t size)
{
	int held = cpu_locked;
	if (held) PosixPause();
	size_t sent = 0;
	while (sent < size)
	{
		ssize_t n = write(fd, buffer + sent, size - sent);
		if (n < 0 && errno == EINTR) continue;
		if (n <= 0) break;
		sent += (size_t)n;
	}
	if (held) PosixResume();
}

void PosixLog(int fd, const char *format, ...)
{
	int saved_errno = errno;
	va_list args, copy;
	va_start(args, format);
	va_copy(copy, args);
	int length = vsnprintf(NULL, 0, format, copy);
	va_end(copy);
	char *text = length < 0 ? NULL : malloc((size_t)length + 1);
	if (text)
	{
		vsnprintf(text, (size_t)length + 1, format, args);
		host_output(fd, text, (size_t)length);
		free(text);
	}
	va_end(args);
	errno = saved_errno;
}

void PosixTerminalLock(void)
{
	PosixPause();
#ifdef PARATOS_IO_TEST
	pthread_mutex_lock(&terminal_mutex);
#else
	if (family_fd >= 0 && PosixGuestMemoryShared()) record_lock(1, F_WRLCK);
#endif
	PosixResume();
}

void PosixTerminalUnlock(void)
{
#ifdef PARATOS_IO_TEST
	pthread_mutex_unlock(&terminal_mutex);
#else
	if (family_fd >= 0 && PosixGuestMemoryShared()) record_lock(1, F_UNLCK);
#endif
}

uint64_t PosixTaskOwner(void) { return 0; }

void PosixRootIdentity(int16_t id, int16_t parent)
{
	guest_pid = id;
	guest_parent_pid = parent;
	runtime_root_id = id;
}

int16_t PosixTaskPid(void) { return guest_pid ? guest_pid : (int16_t)getpid(); }
int16_t PosixTaskParentPid(void) { return guest_pid ? guest_parent_pid : (int16_t)getppid(); }
int16_t PosixRuntimeRootPid(void) { return runtime_root_id ? runtime_root_id : PosixTaskPid(); }

int PosixHostFd(int fd) { return fd == family_fd ? -1 : fd; }
int PosixClose(int fd) { return close(PosixHostFd(fd)); }

int PosixForce(int target, int source)
{
	if (target == family_fd) { errno = EBADF; return -1; }
	return dup2(PosixHostFd(source), target);
}

int PosixAdoptFd(int fd)
{
	if (fd < 0 || fd >= 3) return fd;
	int replacement = fcntl(fd, F_DUPFD, 3);
	int error = errno;
	close(fd);
	errno = error;
	return replacement;
}

ssize_t PosixRead(int fd, void *buffer, size_t count)
{
	void *staging = malloc(count ? count : 1);
	if (!staging) { errno = ENOMEM; return -1; }
	fd = PosixHostFd(fd);
	PosixPause();
	ssize_t result;
	do { result = read(fd, staging, count); } while (result < 0 && errno == EINTR);
	int error = errno;
	PosixResume();
	if (result > 0) memcpy(buffer, staging, (size_t)result);
	free(staging);
	errno = error;
	return result;
}

ssize_t PosixWrite(int fd, const void *buffer, size_t count)
{
	void *snapshot = malloc(count ? count : 1);
	if (!snapshot) { errno = ENOMEM; return -1; }
	memcpy(snapshot, buffer, count);
	fd = PosixHostFd(fd);
	PosixPause();
	ssize_t result;
	do { result = write(fd, snapshot, count); } while (result < 0 && errno == EINTR);
	int error = errno;
	PosixResume();
	free(snapshot);
	errno = error;
	return result;
}

int PosixPoll(struct pollfd *fds, nfds_t count, int timeout)
{
	struct pollfd *native = count ? malloc(count * sizeof(*native)) : NULL;
	if (count && !native) { errno = ENOMEM; return -1; }
	for (nfds_t i = 0; i < count; i++)
	{
		native[i] = fds[i];
		native[i].fd = PosixHostFd(fds[i].fd);
	}
	PosixPause();
	int result = poll(native, count, timeout);
	int error = errno;
	PosixResume();
	for (nfds_t i = 0; i < count; i++) fds[i].revents = native[i].revents;
	free(native);
	errno = error;
	return result;
}

static void fault_handler(int signal_number, siginfo_t *info, void *context)
{
	(void)info;
	(void)context;
	siglongjmp(fault_return, signal_number);
}

static void broken_pipe_handler(int signal_number)
{
	(void)signal_number;
	/* Let the failing write return EPIPE instead of terminating every guest.
	 * Catch rather than SIG_IGN: exec resets caught dispositions to default,
	 * preserving native-program behavior without a process-wide reset window. */
}

/* Called with the execution lock held and returns with it held. */
static void poll_staged(void);
static int run_guest(emuptr32_t basepage)
{
	current_process = basepage;
	InitGuestCPU(basepage);
	finished = 0;
	exit_status = 0;
	while (!finished)
	{
		poll_staged();
		if (sigsetjmp(fault_return, 1))
		{
			/* Format under the execution lock: CPU/disassembler state and
			 * guest memory must stay stable. Write only the host-owned snapshot
			 * after unlocking, so a peer can drain a backpressured output pipe. */
			char *dump = NULL;
			size_t dump_size = 0;
			FILE *out = open_memstream(&dump, &dump_size);
			if (out)
			{
				/* Disassembly can itself touch inaccessible guest memory. */
				if (!sigsetjmp(fault_return, 1)) DumpGuestFault(out);
				fclose(out);
			}
			if (dump)
			{
				host_output(STDOUT_FILENO, dump, dump_size);
				free(dump);
			}
			exit_status = 255;
			break;
		}
		m68k_execute(10000);
		if (!finished && family->running > 1)
		{
			PosixPause();
			sched_yield();
			PosixResume();
		}
	}
	return exit_status;
}

void PosixTerminate(uint16_t status)
{
	exit_status = status;
	finished = 1;
	m68k_end_timeslice();
}

static void sleep_for_child(void)
{
	const struct timespec delay = {0, 1000000};
	PosixPause();
	nanosleep(&delay, NULL);
	PosixResume();
}

static int new_family(uint32_t bp)
{
	stage_family *new = mmap(NULL, sizeof(*new), PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (new == MAP_FAILED) return -1;
	if (family) munmap(family, sizeof(*family));
	if (family_fd >= 0) close(family_fd);
	family = new;
	family_fd = -1;
	self_slot = 0;
	family->entries[0] = (stage){.bp = bp,
		.env = m68k_read_field(bp, basepage_t, p_env), .retained = 1};
	family->running = 1;
	return 0;
}

static int share_family_memory(posix_ram_change *change)
{
	if (PosixGuestMemoryShared()) return 0;

	/* POSIX record locks are per process, so a forked child waits on the
	 * parent's lock even though both inherited the same open descriptor. */
	char path[] = "/tmp/paratos-memory-XXXXXX";
	int fd = mkstemp(path);
	if (fd < 0) return -1;
	if (unlink(path) < 0)
	{
		int error = errno;
		close(fd);
		errno = error;
		return -1;
	}
	fd = PosixAdoptFd(fd);
	if (fd < 0) return -1;
	if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0 || PosixBeginRamChange(1, change) < 0)
	{
		int error = errno;
		close(fd);
		errno = error;
		return -1;
	}
	family_fd = fd;
	cpu_lock(F_WRLCK);
	return 0;
}

int PosixFamilyPidUsed(int16_t pid) { return family && family->reserved[(uint16_t)pid]; }
void PosixFamilyReservePid(int16_t pid, int reserved)
{
	if (family) family->reserved[(uint16_t)pid] = reserved != 0;
}

static void discard_inherited_children(void)
{
	PosixResetPidMap();
	while (children)
	{
		stage_child *next = children->next;
		free(children);
		children = next;
	}
}

int PosixImageBusy(uint32_t address)
{
	if (!family) return 0;
	uint16_t slot = 0;
	do {
		const stage *entry = &family->entries[slot];
		if (entry->retained && (entry->bp == address || entry->env == address)) return 1;
		slot = entry->next;
	} while (slot);
	return 0;
}

int PosixStagedActive(void)
{
	poll_staged();
	if (self_slot || !family) return self_slot != 0;
	for (stage_child *child = children; child; child = child->next)
		if (!child->consumed || family->entries[child->id].retained) return 1;
	return 0;
}

static void finish_stage(uint16_t slot, uint16_t status)
{
	stage *entry = &family->entries[slot];
	if (entry->done) return;
	entry->status = status;
	entry->done = 1;
	family->running--;
	while (entry->done && entry->retained && !entry->dependents)
	{
		entry->retained = 0;
		FreeOwnedMemory(entry->bp);
		if (!slot) break;
		slot = entry->parent;
		entry = &family->entries[slot];
		entry->dependents--;
	}
}

static void poll_staged(void)
{
	for (stage_child **at = &children; *at; )
	{
		stage_child *child = *at;
		stage *entry = &family->entries[child->id];
		if (child->host)
		{
			int status;
			pid_t waited = waitpid(child->host, &status, WNOHANG);
			if (waited == child->host || (waited < 0 && errno == ECHILD))
			{
				child->host = 0;
				finish_stage(child->id, 255);
			}
		}
		if (child->host || !child->consumed || entry->retained)
		{
			at = &child->next;
			continue;
		}

		stage *prev = &family->entries[0];
		while (prev->next != child->id) prev = &family->entries[prev->next];
		prev->next = entry->next;
		*entry = (stage){0};
		*at = child->next;
		PosixReleasePid(child->id);
		free(child);
	}
}

static void close_guest_descriptors(void)
{
	/* An exited supervisor must not keep a pipe writer alive. Keep only
	 * the family lock, used to collect staged descendants and release IDs. */
#ifdef SYS_close_range
	if (family_fd < 0)
	{
		if (!syscall(SYS_close_range, 0u, ~0u, 0u)) return;
	}
	else if (!syscall(SYS_close_range, 0u, (unsigned)family_fd - 1, 0u) &&
		!syscall(SYS_close_range, (unsigned)family_fd + 1, ~0u, 0u)) return;
#endif
	long limit = sysconf(_SC_OPEN_MAX);
	for (long fd = 0; fd < limit; fd++)
		if (fd != family_fd) close((int)fd);
}

static void complete_guest(uint16_t status)
{
	/* Only the calling process owns these descriptors. Any host children
	 * have already inherited their own table. */
	PosixPause();
	PosixTerminalUnlock();
	fflush(stdout);
	fflush(stderr);
	close_guest_descriptors();
	PosixResume();
	finish_stage(self_slot, status);
	for (stage_child *child = children; child; child = child->next) child->consumed = 1;
	PosixOrphanChildren(0);

	/* A staged process can report its guest status before its native
	 * children exit. Remain as their host parent to reap those statuses. */
	for (;;)
	{
		poll_staged();
		int native_pending = PosixReapOrphans();
		if (self_slot ? !children && !native_pending : !family->running) break;
		sleep_for_child();
	}
	PosixPause();
}

static _Noreturn void run_staged(uint16_t slot, int16_t parent)
{
	self_slot = slot;
	guest_pid = slot;
	guest_parent_pid = parent;
	discard_inherited_children();
	PosixResume();
	if (!family->entries[slot].retained) _exit(1);
	int status = run_guest(family->entries[slot].bp);
	complete_guest(status);
	_exit(status & 255);
}

int32_t PosixGo(emuptr32_t bp, int release, int asynchronous)
{
	poll_staged();
	if (bp == current_process || PosixImageBusy(bp)) return TOS_EACCDN;
	int32_t size = MemoryBlockSize(bp);
	if (size < (int32_t)sizeof(basepage_t) + 8) return TOS_EIMBA;
	uint32_t high = m68k_read_field(bp, basepage_t, p_hitpa);
	uint32_t entry = m68k_read_field(bp, basepage_t, p_tbase);
	if (high > bp + (uint32_t)size || high < bp + sizeof(basepage_t) + 8 ||
		entry < 0x10000 || entry >= memory_sz || (entry & 1)) return TOS_ENOEXEC;
	uint32_t env = m68k_read_field(bp, basepage_t, p_env);
	if (release && (!MemoryBlockSize(env) || PosixImageBusy(env))) return TOS_EIMBA;

	stage_child *child = calloc(1, sizeof(*child));
	if (!child) return TOS_ENSMEM;
	int16_t id = PosixReservePid(1);
	if (id < 0) { free(child); return TOS_ENHNDL; }
	posix_ram_change change = {0};
	if (share_family_memory(&change) < 0)
	{
		int32_t error = MapErrno();
		PosixReleasePid(id);
		free(child);
		return error;
	}

	int16_t parent_pid = PosixTaskPid();
	pid_t host = fork();
	if (!host)
	{
		PosixEndRamChange(&change, 1);
		run_staged(id, parent_pid);
	}
	if (host < 0)
	{
		int32_t error = MapErrno();
		if (change.previous)
		{
			close(family_fd);
			family_fd = -1;
			PosixEndRamChange(&change, 0);
		}
		PosixReleasePid(id);
		free(child);
		return error;
	}
	PosixEndRamChange(&change, 1);

	/* The child cannot take the execution lock before this commit. */
	stage *root = &family->entries[0];
	family->entries[id] = (stage){ .bp = bp, .env = env, .next = root->next,
		.parent = self_slot, .retained = 1 };
	root->next = id;
	family->entries[self_slot].dependents++;
	family->running++;
	*child = (stage_child){ .next = children, .host = host, .id = id, .waitable = asynchronous };
	children = child;
	m68k_write_field(bp, basepage_t, p_parent, current_process);
	if (release)
	{
		SetMemoryOwner(env, bp);
		SetMemoryOwner(bp, bp);
	}
	if (asynchronous) return id;

	while (!family->entries[id].done)
	{
		sleep_for_child();
		poll_staged();
	}
	int32_t status = family->entries[id].status;
	child->consumed = 1;
	poll_staged();
	return status;
}

int PosixWaitStage(int16_t pid, emuptr32_t usage, int32_t *result)
{
	poll_staged();
	int pending = 0;
	for (stage_child *child = children; child; child = child->next)
	{
		if (!child->waitable || child->consumed || (pid != -1 && child->id != pid)) continue;
		pending = 1;
		stage *entry = &family->entries[child->id];
		if (!entry->done) continue;
		*result = (int32_t)(((uint32_t)(uint16_t)child->id << 16) | (entry->status & 255));
		if (usage) memset(memory + usage, 0, 8);
		child->consumed = 1;
		poll_staged();
		return 1;
	}
	*result = 0;
	return pending;
}

int32_t PosixForkGuest(int16_t id, int16_t parent)
{
	int channel[2];
	if (pipe(channel) < 0) return MapErrno();
	int shared = PosixGuestMemoryShared();
	pid_t host = fork();
	if (host < 0)
	{
		int32_t result = MapErrno();
		close(channel[0]); close(channel[1]);
		return result;
	}
	if (!host)
	{
		close(channel[0]);
		/* The parent still holds the family lock. No peer can write the
		 * source until this child has copied both RAM and the allocator. */
		int32_t result = 0;
		if ((shared && PosixRemapGuestMemory(0) < 0) || new_family(current_process) < 0)
			result = MapErrno();
		ssize_t sent;
		do { sent = write(channel[1], &result, sizeof(result)); } while (sent < 0 && errno == EINTR);
		close(channel[1]);
		if (result < 0 || sent != (ssize_t)sizeof(result)) _exit(1);
		discard_inherited_children();
		PosixRootIdentity(id, parent);
		return 0;
	}
	close(channel[1]);
	int32_t result;
	ssize_t count;
	do { count = read(channel[0], &result, sizeof(result)); } while (count < 0 && errno == EINTR);
	close(channel[0]);
	if (count == (ssize_t)sizeof(result) && result == 0)
	{
		PosixBindPid(id, host, 0);
		return id;
	}
	if (count != (ssize_t)sizeof(result)) kill(host, SIGKILL);
	while (waitpid(host, NULL, 0) < 0 && errno == EINTR) {}
	return count == (ssize_t)sizeof(result) ? result : TOS_ERROR;
}

int PosixRunRoot(emuptr32_t basepage)
{
	runtime_root_id = PosixTaskPid();
	struct sigaction action = {0};
	action.sa_sigaction = fault_handler;
	action.sa_flags = SA_SIGINFO;
	sigemptyset(&action.sa_mask);
	sigaction(SIGSEGV, &action, NULL);
	sigaction(SIGBUS, &action, NULL);
	struct sigaction broken_pipe = {0};
	broken_pipe.sa_handler = broken_pipe_handler;
	sigemptyset(&broken_pipe.sa_mask);
	if (sigaction(SIGPIPE, &broken_pipe, NULL) < 0 || new_family(basepage) < 0)
	{
		perror("initializing POSIX guest runtime");
		return 1;
	}
	PosixResume();
	int status = run_guest(basepage);
	complete_guest(status);
	return status;
}
