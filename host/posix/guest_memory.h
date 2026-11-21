#pragma once
#include <stdint.h>

#define POSIX_RAM_BYTES (16u * 1024 * 1024)
#define POSIX_HEAP_START UINT32_C(0x10000)
#define POSIX_HEAP_SLOTS ((POSIX_RAM_BYTES - POSIX_HEAP_START) / 4)

typedef struct posix_block {
	uint32_t next, start, size, owner;
	int basepage;
} posix_block;

typedef struct posix_heap {
	uint32_t first, recycled, count;
	posix_block blocks[];
} posix_heap;

typedef struct posix_ram_change {
	uint8_t *previous;
	int was_shared;
} posix_ram_change;

posix_heap *PosixGuestHeap(void);
int PosixGuestMemoryShared(void);
int PosixBeginRamChange(int shared, posix_ram_change *change);
void PosixEndRamChange(posix_ram_change *change, int commit);
int PosixRemapGuestMemory(int shared);
#ifdef PARATOS_MEMORY_TEST
void PosixTestMappingFailure(int count);
#endif
