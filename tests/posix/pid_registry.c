#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

static int allocation_fails, forks, waits;
static pid_t fork_result = 1000, last_wait;
static int child_exited;
static void *registry_malloc(size_t size)
{
	return allocation_fails ? NULL : malloc(size);
}
static pid_t registry_fork(void) { forks++; return fork_result; }
static pid_t registry_wait(pid_t pid, int *status, int flags, struct rusage *usage);

/* Inject failures at the registry's allocation boundary, before a real child
 * could be created. Unrelated syscall implementations are linker-discarded. */
#define malloc registry_malloc
#define fork registry_fork
#define wait4 registry_wait
#include "../../host/posix/proc.c"
#undef malloc
#undef fork
#undef wait4

uint8_t *memory;
size_t memory_sz;

static uint64_t owner = 42;
static int16_t reported_id, reported_parent;
uint64_t PosixTaskOwner(void) { return owner; }
int16_t PosixTaskPid(void) { return 7; }
int16_t PosixTaskParentPid(void) { return 8; }
int16_t PosixRuntimeRootPid(void) { return 1; }
int PosixHasThreads(void) { return 0; }
void PosixRootIdentity(int16_t id, int16_t parent) { reported_id = id; reported_parent = parent; }
int32_t MapErrno(void) { return TOS_ERROR; }
void PosixPause(void) {}
void PosixResume(void) {}
int PosixWaitThread(int16_t pid, int16_t flags, emuptr32_t usage, int32_t *result)
{
	(void)pid; (void)flags; (void)usage; (void)result;
	return 0;
}
static pid_t registry_wait(pid_t pid, int *status, int flags, struct rusage *usage)
{
	(void)status; (void)flags; (void)usage;
	assert(pid > 0); /* Never wait for an arbitrary host-process child. */
	last_wait = pid;
	waits++;
	if (child_exited) { *status = 0; return pid; }
	return 0;
}

int main(void)
{
	allocation_fails = 1;
	assert(PosixReservePid(1) < 0);
	assert(Pfork() == TOS_ENHNDL);
	assert(forks == 0);
	allocation_fails = 0;
	int16_t synchronous = PosixReservePid(1);
	assert(synchronous != PosixRuntimeRootPid());
	PosixBindPid(synchronous, 1234, 1);
	assert(Pwaitpid(-1, 1, 0) == TOS_ECHILD);
	assert(waits == 0);
	PosixBindPid(synchronous, 1234, 0);
	owner = 43;
	assert(Pwaitpid(-1, 1, 0) == TOS_ECHILD);
	assert(Pwaitpid(synchronous, 1, 0) == TOS_ECHILD);
	assert(waits == 0);
	owner = 42;
	assert(Pwaitpid(-1, 1, 0) == 0);
	assert(waits == 1 && last_wait == 1234);
	PosixReleasePid(synchronous);
	/* A terminated owner's native child remains reserved while still running,
	 * then releases its entry only after the central reaper collects status. */
	for (int i = 0; i < 50; i++)
	{
		child_exited = 0;
		int16_t orphan = PosixReservePid(0);
		assert(orphan > 0 && orphan != PosixRuntimeRootPid());
		PosixBindPid(orphan, 2345, 0);
		assert(PosixOrphanChildren(owner));
		assert(PosixReapOrphans() == 1);
		assert(pid_map_head && pid_map_head->tos_pid == orphan);
		child_exited = 1;
		assert(PosixReapOrphans() == 0);
		assert(pid_map_head == NULL);
	}
	fork_result = 0;
	assert(Pfork() == 0);
	assert(reported_id > 0 && reported_parent == 7);
	assert(pid_map_head == NULL);
	return 0;
}
