#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>

#include "gemdos.h"
#include "host.h"
#include "runtime.h"
#include "tasks.h"
#include "../../guest_tasks.h"
#include "../../guest_exec.h"

static pthread_mutex_t cpu_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t terminal_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static _Thread_local sigjmp_buf fault_return;
static _Thread_local int finished, exit_status;
static _Thread_local int cpu_locked;

typedef struct task {
	guest_task guest;
	uint64_t generation;
	int token;
	int streams[3];
} task;
static guest_tasks guests;
static guest_task initial;
static _Thread_local task *self;
static _Thread_local int root_streams[3] = {0, 1, 2};
static int next_host_id = -2;
static uint64_t next_generation = 1;
static _Thread_local int16_t root_id, root_parent_id;
static int16_t runtime_root_id;
static guest_thread reaper_thread;
static int reaper_started, reaper_running, reaper_stop;

void PosixPause(void)
{
	cpu_locked = 0;
	pthread_mutex_unlock(&cpu_mutex);
}

void PosixResume(void)
{
	pthread_mutex_lock(&cpu_mutex);
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
	pthread_mutex_lock(&terminal_mutex);
	PosixResume();
}

void PosixTerminalUnlock(void)
{
	pthread_mutex_unlock(&terminal_mutex);
}

uint64_t PosixTaskOwner(void)
{
	return self ? self->generation : 0;
}

void PosixRootIdentity(int16_t id, int16_t parent)
{
	root_id = id;
	root_parent_id = parent;
	runtime_root_id = id;
}

int16_t PosixRuntimeRootPid(void)
{
	return runtime_root_id ? runtime_root_id : PosixTaskPid();
}

static void *reap_children(void *argument)
{
	(void)argument;
	PosixResume();
	while (PosixReapOrphans() && !reaper_stop)
	{
		struct timespec until;
		clock_gettime(CLOCK_REALTIME, &until);
		until.tv_nsec += 10000000;
		if (until.tv_nsec >= 1000000000) { until.tv_sec++; until.tv_nsec -= 1000000000; }
		pthread_cond_timedwait(&changed, &cpu_mutex, &until);
	}
	reaper_running = 0;
	pthread_cond_broadcast(&changed);
	PosixPause();
	return NULL;
}

static void join_finished_reaper(void)
{
	if (reaper_started && !reaper_running)
	{
		guest_thread_join(&reaper_thread);
		reaper_started = 0;
	}
}

static void handoff_children(uint64_t owner)
{
	if (!PosixOrphanChildren(owner)) return;
	join_finished_reaper();
	if (!reaper_started)
	{
		reaper_running = 1;
		if (!guest_thread_start(&reaper_thread, reap_children, NULL)) reaper_started = 1;
		else reaper_running = 0; /* retain records; scheduler polling retries collection */
	}
	pthread_cond_broadcast(&changed);
}

static int *streams(void)
{
	return self ? self->streams : root_streams;
}

int PosixHostFd(int fd)
{
	return fd >= 0 && fd < 3 ? streams()[fd] : fd;
}

int PosixClose(int fd)
{
	int native = PosixHostFd(fd);
	if (fd >= 0 && fd < 3) streams()[fd] = -1;
	return close(native);
}

int PosixForce(int target, int source)
{
	if (target < 0 || target >= 3) return dup2(PosixHostFd(source), target);
	int replacement = fcntl(PosixHostFd(source), F_DUPFD_CLOEXEC, 6);
	if (replacement < 0) return -1;
	PosixClose(target);
	streams()[target] = replacement;
	return 0;
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

void PosixExportStandardHandles(void)
{
	for (int fd = 0; fd < 3; fd++)
	{
		int native = PosixHostFd(fd);
		if (native < 0) close(fd);
		else if (native != fd) dup2(native, fd);
	}
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

/* Called with cpu_mutex held; returns with it held as well. */
static int run_guest(emuptr32_t basepage)
{
	current_process = basepage;
	InitGuestCPU(basepage);
	finished = 0;
	exit_status = 0;
	while (!finished)
	{
		PosixReapOrphans();
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
		if (!finished)
		{
			PosixPause();
			guest_thread_yield();
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

static guest_task *current_guest(void)
{
	return self ? &self->guest : &initial;
}

static int32_t join_task(guest_task *guest)
{
	PosixPause();
	int32_t result = guest_thread_join(&guest->thread);
	PosixResume();
	if (result < 0) return result;
	result = guest->status;
	PosixForgetPid(((task *)guest)->token);
	guest_task_reap(&guests, guest);
	pthread_cond_broadcast(&changed);
	return result;
}

static void *thread_main(void *argument)
{
	task *t = argument;
	self = t;
	PosixResume();
	int status = run_guest(t->guest.basepage);
	handoff_children(t->generation);
	for (int fd = 0; fd < 3; fd++) PosixClose(fd);
	guest_task_finish(&guests, &t->guest, status);
	pthread_cond_broadcast(&changed);
	PosixPause();
	return NULL;
}

int PosixHasThreads(void)
{
	join_finished_reaper();
	return guest_task_next_join(&guests) != NULL || reaper_started;
}

int16_t PosixTaskPid(void)
{
	return self ? self->guest.pid : (root_id ? root_id : (int16_t)getpid());
}

int16_t PosixTaskParentPid(void)
{
	return self ? self->guest.parent_pid : (root_id ? root_parent_id : (int16_t)getppid());
}

int32_t PosixGo(emuptr32_t bp, int release, int asynchronous)
{
	if (bp == current_process || guest_task_image_busy(&guests, bp, 1)) return TOS_EACCDN;
	int32_t size = MemoryBlockSize(bp);
	if (size < (int32_t)sizeof(basepage_t) + 8) return TOS_EIMBA;
	uint32_t high = m68k_read_field(bp, basepage_t, p_hitpa);
	uint32_t entry = m68k_read_field(bp, basepage_t, p_tbase);
	if (high > bp + (uint32_t)size || high < bp + sizeof(basepage_t) + 8 ||
		entry < 0x10000 || entry >= memory_sz || (entry & 1)) return TOS_ENOEXEC;
	task *t = calloc(1, sizeof(*t));
	if (!t) return TOS_ENSMEM;
	/* Synthetic host IDs share the same 16-bit guest namespace as forked children. */
	t->token = next_host_id--;
	int16_t pid = PosixRegisterPid(t->token);
	if (pid < 0) { free(t); return TOS_ENHNDL; }
	t->generation = next_generation++;
	for (int fd = 0; fd < 3; fd++) t->streams[fd] = -1;
	for (int fd = 0; fd < 3; fd++)
	{
		int native = PosixHostFd(fd);
		if (native < 0) continue;
		t->streams[fd] = fcntl(native, F_DUPFD_CLOEXEC, 6);
		if (t->streams[fd] < 0)
		{
			for (int i = 0; i < fd; i++) if (t->streams[i] >= 0) close(t->streams[i]);
			PosixForgetPid(t->token);
			free(t);
			return TOS_ENHNDL;
		}
	}
	int32_t error = guest_task_start(&guests, current_guest(), &t->guest,
		bp, pid, asynchronous, thread_main);
	if (error < 0)
	{
		for (int fd = 0; fd < 3; fd++) if (t->streams[fd] >= 0) close(t->streams[fd]);
		PosixForgetPid(t->token);
		free(t);
		return error;
	}
	m68k_write_field(bp, basepage_t, p_parent, current_process);
	if (release)
	{
		SetMemoryOwner(m68k_read_field(bp, basepage_t, p_env), bp);
		SetMemoryOwner(bp, bp);
	}
	return asynchronous ? pid : join_task(&t->guest);
}

int PosixWaitThread(int16_t pid, int16_t flags, emuptr32_t usage, int32_t *result)
{
	guest_task *child = guest_task_wait_child(&guests, current_guest(), pid);
	if (!child) return 0;
	if (!child->done && (flags & 1)) { *result = 0; return 1; }

	int16_t child_pid = child->pid;
	int32_t status = join_task(child);
	*result = status < 0 ? status : guest_wait_status(child_pid, status & 255);
	if (usage && status >= 0) memset(memory + usage, 0, 8);
	return 1;
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
	if (sigaction(SIGPIPE, &broken_pipe, NULL) < 0)
	{
		perror("installing host SIGPIPE handler");
		return 1;
	}
	PosixResume();
	guest_tasks_init(&guests, &initial, basepage, runtime_root_id, FreeOwnedMemory);
	int status = run_guest(basepage);
	/* Flush host diagnostics before releasing root-owned stream mappings.
	 * Drop the execution mutex in case a surviving guest must drain a pipe. */
	PosixPause();
	fflush(stdout);
	fflush(stderr);
	PosixResume();
	for (int fd = 0; fd < 3; fd++) PosixClose(fd);
	handoff_children(0);
	/* Ending the initial guest must not terminate still-running guest threads. */
	guest_task_finish(&guests, &initial, status);
	guest_task *child;
	while ((child = guest_task_next_join(&guests)))
	{
		/* A running guest may itself be joining this child. Wait for
		 * every guest to finish before taking over their remaining joins. */
		if (guest_task_have_peer(&guests, &initial))
		{
			pthread_cond_wait(&changed, &cpu_mutex);
			continue;
		}
		if (join_task(child) < 0) break;
	}
	/* Do not wait for still-running detached native children at host shutdown.
	 * Collect completed ones, then let the OS adopt any that outlive us. */
	PosixReapOrphans();
	reaper_stop = 1;
	pthread_cond_broadcast(&changed);
	PosixPause();
	if (reaper_started) guest_thread_join(&reaper_thread);
	return status;
}
