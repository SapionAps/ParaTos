#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tasks.h"

uint8_t *memory;
size_t memory_sz;
static int reads, writes;

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
	memory = calloc(1, memory_sz);
	assert(memory);
	PosixResume();
	memset(memory, 'A', 4);
	assert(PosixWrite(1, memory, 4) == 4);
	assert(writes == 1 && !memcmp(memory, "BBBB", 4));
	assert(PosixRead(0, memory, 4) == 4);
	assert(reads == 1 && !memcmp(memory, "ZZZZ", 4));
	assert(PosixRead(0, memory + 15, 4) == -1);
	assert(PosixWrite(1, memory + 15, 4) == -1);
	assert(reads == 1 && writes == 1);
	PosixPause();
	free(memory);
	return 0;
}
