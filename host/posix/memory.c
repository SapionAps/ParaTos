#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>

#include "common.h"
#include "host.h"

int page_size;

#if __x86_64__
// Allocate the entire address space addressable by a 68020 processor on 64 bit systems
#define MAX_MEM 0x100000000LL
#else
// ... less on 32 bit (512Mb)
#define MAX_MEM 0x20000000
#endif

void InitM68KMemory(void)
{
	page_size = getpagesize();

	// Reserve entire address space upfront. That way we can grow allocated memory without having to reallocate it
	memory = mmap(NULL, MAX_MEM, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (memory == MAP_FAILED)
	{
		perror("Mapping 68k memory failed: ");
		exit(1);
	}

	// Actually allocate first 14 Mb of that address space
	memory_sz = 16 * 1024 * 1024;
	mprotect(memory, memory_sz, PROT_READ|PROT_WRITE);

	// Some programs seem to want to play with some high addresses
	mprotect(memory + 0xff000000, 0x01000000, PROT_READ|PROT_WRITE);
}

