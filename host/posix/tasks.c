#include <pthread.h>
#include <sched.h>
#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include "gemdos.h"
#include "host.h"
#include "runtime.h"
#include "tasks.h"

static pthread_mutex_t cpu_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t terminal_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static _Thread_local sigjmp_buf fault_return;
static _Thread_local int finished, exit_status;

typedef struct task {
	struct task *next;
	pthread_t thread;
	emuptr32_t basepage, parent;
	uint64_t generation, parent_generation;
	int id, token, parent_id, asynchronous, done, status, cleanup_pending;
	int streams[3];
} task;
static task *tasks, *retired;
static _Thread_local task *self;
static _Thread_local int root_streams[3] = {0, 1, 2};
static int next_host_id = -2;
static uint64_t next_generation = 1;
static _Thread_local int16_t root_id, root_parent_id;

void PosixPause(void)
{
	pthread_mutex_unlock(&cpu_mutex);
}

void PosixResume(void)
{
	pthread_mutex_lock(&cpu_mutex);
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

static int valid_io_buffer(const void *buffer, size_t count)
{
	uintptr_t at = (uintptr_t)buffer, base = (uintptr_t)memory;
	if (at >= base && at - base < UINT64_C(0x100000000))
		return at - base < memory_sz && count <= memory_sz - (at - base);
	return 1; /* host-owned scalar buffers are also used by console calls */
}

ssize_t PosixRead(int fd, void *buffer, size_t count)
{
	if (!valid_io_buffer(buffer, count)) { errno = EFAULT; return -1; }
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
	if (!valid_io_buffer(buffer, count)) { errno = EFAULT; return -1; }
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

/* Called with cpu_mutex held; returns with it held as well. */
static int run_guest(emuptr32_t basepage)
{
	current_process = basepage;
	InitGuestCPU(basepage);
	finished = 0;
	exit_status = 0;
	while (!finished)
	{
		if (sigsetjmp(fault_return, 1))
		{
			/* Disassembly can itself touch inaccessible guest memory. */
			if (!sigsetjmp(fault_return, 1)) DumpGuestFault();
			exit_status = 255;
			break;
		}
		m68k_execute(10000);
		if (!finished)
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

static int has_dependents(emuptr32_t bp)
{
	for (task *t = tasks; t; t = t->next)
		if (t->parent == bp && (!t->done || t->cleanup_pending)) return 1;
	for (task *t = retired; t; t = t->next)
		if (t->parent == bp && t->cleanup_pending) return 1;
	return 0;
}

static void reclaim_memory(void)
{
	int progress;
	do {
		progress = 0;
		for (task *t = tasks; t; t = t->next)
			if (t->done && t->cleanup_pending && !has_dependents(t->basepage))
			{
				FreeOwnedMemory(t->basepage);
				t->cleanup_pending = 0;
				progress = 1;
			}
		for (task **at = &retired; *at; )
		{
			task *t = *at;
			if (has_dependents(t->basepage)) { at = &t->next; continue; }
			FreeOwnedMemory(t->basepage);
			*at = t->next;
			free(t);
			progress = 1;
		}
	} while (progress);
}

static void remove_task(task *t)
{
	task **at = &tasks;
	while (*at != t) at = &(*at)->next;
	*at = t->next;
	PosixForgetPid(t->token);
	if (t->cleanup_pending) { t->next = retired; retired = t; }
	else free(t);
	reclaim_memory();
	pthread_cond_broadcast(&changed);
}

static void *thread_main(void *argument)
{
	task *t = argument;
	self = t;
	pthread_mutex_lock(&cpu_mutex);
	t->status = run_guest(t->basepage);
	for (int fd = 0; fd < 3; fd++) PosixClose(fd);
	t->done = 1;
	reclaim_memory();
	pthread_cond_broadcast(&changed);
	pthread_mutex_unlock(&cpu_mutex);
	return NULL;
}

int PosixHasThreads(void)
{
	return tasks != NULL;
}

int16_t PosixTaskPid(void)
{
	return self ? (int16_t)self->id : (root_id ? root_id : (int16_t)getpid());
}

int16_t PosixTaskParentPid(void)
{
	return self ? (int16_t)self->parent_id : (root_id ? root_parent_id : (int16_t)getppid());
}

int32_t PosixGo(emuptr32_t bp, int release, int asynchronous)
{
	int32_t size = MemoryBlockSize(bp);
	if (size < (int32_t)sizeof(basepage_t) + 8) return TOS_EIMBA;
	uint32_t high = m68k_read_field(bp, basepage_t, p_hitpa);
	uint32_t entry = m68k_read_field(bp, basepage_t, p_tbase);
	if (high > bp + (uint32_t)size || high < bp + sizeof(basepage_t) + 8 ||
		entry < 0x10000 || entry >= memory_sz || (entry & 1)) return TOS_ENOEXEC;
	for (task *t = tasks; t; t = t->next)
		if (t->basepage == bp) return TOS_EACCDN;
	for (task *t = retired; t; t = t->next)
		if (t->basepage == bp) return TOS_EACCDN;
	if (bp == current_process) return TOS_EACCDN;
	task *t = calloc(1, sizeof(*t));
	if (!t) return TOS_ENSMEM;
	/* Synthetic host IDs share the same 16-bit guest namespace as forked children. */
	t->token = next_host_id--;
	t->id = PosixRegisterPid(t->token);
	if (t->id < 0) { free(t); return TOS_ENHNDL; }
	t->basepage = bp;
	t->parent = current_process;
	t->generation = next_generation++;
	t->parent_generation = PosixTaskOwner();
	t->parent_id = PosixTaskPid();
	t->asynchronous = asynchronous;
	t->cleanup_pending = 1;
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
	t->next = tasks;
	tasks = t;
	m68k_write_field(bp, basepage_t, p_parent, current_process);
	int error = pthread_create(&t->thread, NULL, thread_main, t);
	if (error)
	{
		for (int fd = 0; fd < 3; fd++) if (t->streams[fd] >= 0) close(t->streams[fd]);
		t->cleanup_pending = 0;
		remove_task(t);
		return TOS_ENSMEM;
	}
	if (release)
	{
		SetMemoryOwner(m68k_read_field(bp, basepage_t, p_env), bp);
		SetMemoryOwner(bp, bp);
	}
	if (asynchronous) return t->id;

	PosixPause();
	pthread_join(t->thread, NULL);
	PosixResume();
	int32_t status = t->status;
	remove_task(t);
	return status;
}

int PosixWaitThread(int16_t pid, int16_t flags, emuptr32_t usage, int32_t *result)
{
	task *selected = NULL;
	for (task *t = tasks; t; t = t->next)
		if (t->asynchronous && t->parent_generation == PosixTaskOwner() && (pid == -1 || pid == t->id))
		{
			selected = t;
			if (t->done) break;
		}
	if (!selected) return 0;
	if (!selected->done && (flags & 1)) { *result = 0; return 1; }

	PosixPause();
	pthread_join(selected->thread, NULL);
	PosixResume();
	*result = (int32_t)(((uint32_t)selected->id << 16) | (selected->status & 255));
	if (usage) memset(memory + usage, 0, 8);
	remove_task(selected);
	return 1;
}

int PosixRunRoot(emuptr32_t basepage)
{
	struct sigaction action = {0};
	action.sa_sigaction = fault_handler;
	action.sa_flags = SA_SIGINFO;
	sigemptyset(&action.sa_mask);
	sigaction(SIGSEGV, &action, NULL);
	pthread_mutex_lock(&cpu_mutex);
	int status = run_guest(basepage);
	/* Ending the initial guest must not terminate still-running guest threads. */
	while (tasks)
	{
		task *t = tasks;
		while (t && t->done) t = t->next;
		if (t) { pthread_cond_wait(&changed, &cpu_mutex); continue; }
		/* No guest is still running or waiting on a join. */
		t = tasks;
		pthread_join(t->thread, NULL);
		remove_task(t);
	}
	pthread_mutex_unlock(&cpu_mutex);
	return status;
}
