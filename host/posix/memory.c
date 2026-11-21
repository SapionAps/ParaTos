#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "common.h"
#include "host.h"
#include "guest_memory.h"

int page_size;
static int shared_ram;

#if UINTPTR_MAX > UINT32_MAX
#define MAX_MEM ((size_t)UINT64_C(0x100000000))
#else
#define MAX_MEM ((size_t)0x20000000)
#endif

/* The allocator is outside the guest's addressable reservation. Its
 * links are indices so the host mapping can move as a single transaction. */
#define HEAP_BYTES (sizeof(posix_heap) + ((size_t)POSIX_HEAP_SLOTS + 1) * sizeof(posix_block))
#define MAPPING_BYTES (MAX_MEM + HEAP_BYTES)

#ifdef PARATOS_MEMORY_TEST
static int fail_mapping;
void PosixTestMappingFailure(int count) { fail_mapping = count; }
#endif

static int map_window(uint8_t *at, size_t size, int kind)
{
#ifdef PARATOS_MEMORY_TEST
	if (fail_mapping && !--fail_mapping) { errno = ENOMEM; return -1; }
#endif
	return mmap(at, size, PROT_READ | PROT_WRITE,
		kind | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED ? -1 : 0;
}

static uint8_t *create_mapping(int shared)
{
	/* Back only the accessible windows. The anonymous reservation between
	 * them retains PROT_NONE even on hosts with larger page sizes. */
	uint8_t *at = mmap(NULL, MAPPING_BYTES, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (at == MAP_FAILED) return NULL;
	int kind = shared ? MAP_SHARED : MAP_PRIVATE;
	if (map_window(at, POSIX_RAM_BYTES, kind) < 0 ||
		(MAX_MEM > UINT32_MAX && map_window(at + UINT32_C(0xff000000), POSIX_RAM_BYTES, kind) < 0) ||
		map_window(at + MAX_MEM, HEAP_BYTES, kind) < 0)
	{
		int error = errno;
		munmap(at, MAPPING_BYTES);
		errno = error;
		return NULL;
	}
	return at;
}

posix_heap *PosixGuestHeap(void)
{
	return (posix_heap *)(memory + MAX_MEM);
}

int PosixGuestMemoryShared(void) { return shared_ram; }

int PosixBeginRamChange(int shared, posix_ram_change *change)
{
	*change = (posix_ram_change){0};
	if (shared_ram == shared) return 0;
	uint8_t *replacement = create_mapping(shared);
	if (!replacement) return -1;

	memcpy(replacement, memory, POSIX_RAM_BYTES);
	if (MAX_MEM > UINT32_MAX)
		memcpy(replacement + UINT32_C(0xff000000), memory + UINT32_C(0xff000000), POSIX_RAM_BYTES);
	size_t heap_bytes = sizeof(posix_heap) + ((size_t)PosixGuestHeap()->count + 1) * sizeof(posix_block);
	memcpy(replacement + MAX_MEM, memory + MAX_MEM, heap_bytes);

	*change = (posix_ram_change){ memory, shared_ram };
	memory = replacement;
	shared_ram = shared;
	return 0;
}

void PosixEndRamChange(posix_ram_change *change, int commit)
{
	if (!change->previous) return;
	if (commit) munmap(change->previous, MAPPING_BYTES);
	else
	{
		munmap(memory, MAPPING_BYTES);
		memory = change->previous;
		shared_ram = change->was_shared;
	}
	change->previous = NULL;
}

int PosixRemapGuestMemory(int shared)
{
	posix_ram_change change;
	int result = PosixBeginRamChange(shared, &change);
	if (!result) PosixEndRamChange(&change, 1);
	return result;
}

void InitM68KMemory(void)
{
	page_size = getpagesize();
	memory = create_mapping(0);
	if (!memory)
	{
		perror("Mapping 68k memory failed");
		exit(1);
	}
	memory_sz = POSIX_RAM_BYTES;
	shared_ram = 0;
}
