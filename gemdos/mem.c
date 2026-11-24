#include <stdlib.h>
#include "common.h"
#include "gemdos.h"
#include "tos_errors.h"
#include "sysvars.h"

#include "../host/posix/guest_memory.h"
#include "../host/posix/tasks.h"

void InitMemory(void)
{
	m68k_write_memory_32(_membot, POSIX_HEAP_START);
	m68k_write_memory_32(_memtop, memory_sz - 1);
	m68k_write_memory_32(phystop, memory_sz);
	*PosixGuestHeap() = (posix_heap){0};
}

/**
 * Maddalt - 20
 *
 * The GEMDOS routine Maddalt permits the inclusion of a block of additional
 * alternate-RAM in the GEMDOS memory list that would not normally have been
 * identified by the system. The following apply: Parameter Meaning
 *
 * int32_t Maddalt ( void *start, int32_t size )
 */
int32_t Maddalt ( emuptr32_t start, int32_t size )
{
	NOT_IMPLEMENTED(GEMDOS, Maddalt, 20);
	return TOS_ENOSYS;
}

/**
 * Malloc - 72
 *
 * The GEMDOS routine Malloc reserves a block of memory for an application,
 * or calculates the size of the largest free block of memory. The following
 * applies for the parameter number: Value Meaning
 *
 * void *Malloc ( int32_t number )
 */
emuptr32_t Malloc(int32_t size)
{
	if (size != -1 && (size <= 0 || (uint32_t)size > memory_sz - POSIX_HEAP_START))
		return 0;
	uint32_t request = size == -1 ? 0 : ((uint32_t)size + 3) & ~3u;
	uint32_t start = POSIX_HEAP_START, largest = 0;
	posix_heap *heap = PosixGuestHeap();
	uint32_t *at = &heap->first;
	/* All gaps before the rover were smaller than skipped + 1. */
	if (request > heap->skipped && heap->rover)
	{
		posix_block *previous = &heap->blocks[heap->rover];
		start = previous->start + previous->size;
		at = &previous->next;
		largest = heap->skipped;
	}
	for (;;)
	{
		posix_block *next = *at ? &heap->blocks[*at] : NULL;
		uint32_t end = next ? next->start : (uint32_t)memory_sz;
		uint32_t gap = end - start;
		if (request && gap >= request)
		{
			uint32_t slot = heap->recycled;
			if (slot) heap->recycled = heap->blocks[slot].next;
			else
			{
				if (heap->count >= POSIX_HEAP_SLOTS) return 0;
				slot = ++heap->count;
			}
			heap->blocks[slot] = (posix_block){*at, start, request, current_process, 0};
			*at = slot;
			heap->rover = slot;
			heap->skipped = largest;
			return start;
		}
		if (gap > largest) largest = gap;
		if (!next) return request ? 0 : largest;
		start = next->start + next->size;
		at = &next->next;
	}
}

static uint32_t *find_block(posix_heap *heap, emuptr32_t address)
{
	for (uint32_t *at = &heap->first; *at; at = &heap->blocks[*at].next)
		if (heap->blocks[*at].start == address) return at;
	return NULL;
}

static void release_block(posix_heap *heap, uint32_t *at)
{
	heap->rover = 0;
	uint32_t slot = *at;
	*at = heap->blocks[slot].next;
	heap->blocks[slot].next = heap->recycled;
	heap->recycled = slot;
}

int32_t Mfree(emuptr32_t address)
{
	posix_heap *heap = PosixGuestHeap();
	uint32_t *at = find_block(heap, address);
	if (!at || (PosixGuestMemoryShared() && PosixImageBusy(address))) return TOS_EIMBA;
	release_block(heap, at);
	return 0;
}

int32_t MemoryBlockSize(emuptr32_t address)
{
	posix_heap *heap = PosixGuestHeap();
	uint32_t *at = find_block(heap, address);
	return at ? (int32_t)heap->blocks[*at].size : 0;
}

void MarkBasepage(emuptr32_t address)
{
	posix_heap *heap = PosixGuestHeap();
	uint32_t *at = find_block(heap, address);
	if (at) heap->blocks[*at].basepage = 1;
}

void SetMemoryOwner(emuptr32_t address, emuptr32_t owner)
{
	posix_heap *heap = PosixGuestHeap();
	uint32_t *at = find_block(heap, address);
	if (at) heap->blocks[*at].owner = owner;
}

void FreeOwnedMemory(emuptr32_t owner)
{
	posix_heap *heap = PosixGuestHeap();
	for (uint32_t *at = &heap->first; *at; )
	{
		posix_block *block = &heap->blocks[*at];
		if (block->owner == owner) release_block(heap, at);
		else at = &block->next;
	}
}

int32_t Mshrink(emuptr32_t address, int32_t size)
{
	if (!size) return Mfree(address);
	if (size < 0 || (uint32_t)size > memory_sz) return TOS_EINVAL;
	posix_heap *heap = PosixGuestHeap();
	uint32_t *at = find_block(heap, address);
	if (!at) return TOS_EIMBA;
	posix_block *block = &heap->blocks[*at];
	uint32_t request = ((uint32_t)size + 3) & ~3u;
	if (PosixGuestMemoryShared() && PosixImageBusy(address) &&
		(address != current_process || request < sizeof(basepage_t) + 8))
		return TOS_EIMBA;
	if (request > block->size) return TOS_EGSBF;
	if (request < block->size) heap->rover = 0;
	block->size = request;
	if (block->basepage && request >= sizeof(basepage_t))
		m68k_write_field(address, basepage_t, p_hitpa, address + request);
	return request;
}

/**
 * Mxalloc - 68
 *
 * The GEMDOS routine Mxalloc reserves a block of memory of the size amount.
 * One can use the bits of the WORD parameter mode to specify the desired
 * type of memory. The following apply: Bits Meaning
 *
 * void *Mxalloc ( int32_t amount, int16_t mode )
 */
emuptr32_t Mxalloc ( int32_t amount, int16_t mode )
{
	NOT_IMPLEMENTED(GEMDOS, Mxalloc, 68);
	return TOS_ENOSYS;
}
