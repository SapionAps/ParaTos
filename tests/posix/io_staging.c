#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "tasks.h"
#include "gemdos.h"

uint8_t *memory;
size_t memory_sz;
static int reads, writes, signal_waits;
int32_t MapErrno(void) { assert(errno == EINTR); return TOS_ERROR; }

int __wrap_pause(void)
{
	PosixResume(); /* A peer must be able to run while this guest is waiting. */
	signal_waits++;
	PosixPause();
	errno = EINTR;
	return -1;
}

int __wrap_sigsuspend(const sigset_t *mask)
{
	assert(sigismember(mask, SIGPIPE) == 1);
	return __wrap_pause();
}

ssize_t __wrap_read(int fd, void *buffer, size_t count)
{
	assert(fd == 0 && count == 4 && buffer != memory);
	/* Simulate a second guest acquiring the execution lock while I/O runs. */
	PosixResume();
	memset(memory, 'B', count);
	memset(buffer, 'Z', count);
	PosixPause();
	reads++;
	return (ssize_t)count;
}

ssize_t __wrap_write(int fd, const void *buffer, size_t count)
{
	assert(fd == 1 && count == 4 && buffer != memory);
	PosixResume();
	memset(memory, 'B', count);
	assert(!memcmp(buffer, "AAAA", count));
	PosixPause();
	writes++;
	return (ssize_t)count;
}

int main(void)
{
	memory_sz = 16;
	memory = calloc(1, 64);
	assert(memory);
	PosixResume();
	memset(memory, 'A', 4);
	assert(PosixWrite(1, memory, 4) == 4);
	assert(writes == 1 && !memcmp(memory, "BBBB", 4));
	assert(PosixRead(0, memory, 4) == 4);
	assert(reads == 1 && !memcmp(memory, "ZZZZ", 4));
	/* A host-owned buffer may be near the guest mapping in virtual address
	 * space. Generic I/O must not infer buffer ownership from its address. */
	memcpy(memory + 32, "AAAA", 4);
	assert(PosixWrite(1, memory + 32, 4) == 4);
	assert(PosixRead(0, memory + 32, 4) == 4);
	assert(!memcmp(memory + 32, "ZZZZ", 4));
	assert(Fread(0, 4, 15) == TOS_EINVAL);
	assert(Fwrite(1, 4, 15) == TOS_EINVAL);
	assert(reads == 2 && writes == 2);
	errno = ERANGE;
	PosixLog(1, "%s", "AAAA");
	assert(errno == ERANGE);
	assert(writes == 3); /* host diagnostics also let a peer acquire the lock */
	Pause();
	assert(Psigpause(1 << 13) == TOS_ERROR);
	assert(signal_waits == 2);
	PosixPause();
	PosixLog(1, "%s", "AAAA"); /* startup diagnostics have no execution lock */
	assert(writes == 4);
	free(memory);
	return 0;
}
