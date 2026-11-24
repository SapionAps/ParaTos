/* Used by the Linux integration tests. Keep failure controls out of the
 * emulator binary and inject at the native mmap/fork/exit boundaries. */
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

static pid_t initial_pid;
static int forks, shared_windows, snapshot_windows;

__attribute__((constructor)) static void init(void)
{
	initial_pid = getpid();
}

static int fault(const char *name)
{
	const char *requested = getenv("PARATOS_TEST_FAULT");
	return requested && !strcmp(requested, name);
}

pid_t fork(void)
{
	pid_t (*native_fork)(void) = dlsym(RTLD_NEXT, "fork");
	if (fault("fork") && !forks++)
	{
		errno = EAGAIN;
		return -1;
	}
	return native_fork();
}

void *mmap(void *address, size_t length, int prot, int flags, int fd, off_t offset)
{
	void *(*native_map)(void *, size_t, int, int, int, off_t) = dlsym(RTLD_NEXT, "mmap");
	if ((flags & MAP_FIXED) && fd == -1 &&
		(((flags & MAP_SHARED) && fault("shared-map") && ++shared_windows == 2) ||
		 ((flags & MAP_PRIVATE) && getpid() != initial_pid && fault("snapshot") &&
		  ++snapshot_windows == 2)))
	{
		errno = ENOMEM;
		return MAP_FAILED;
	}
	return native_map(address, length, prot, flags, fd, offset);
}

_Noreturn void _exit(int status)
{
	void (*native_exit)(int) = dlsym(RTLD_NEXT, "_exit");
	if (getpid() != initial_pid && fault("stage-exit"))
	{
		struct timespec delay = {0, 100000000};
		while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {}
	}
	native_exit(status);
	abort();
}
