#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include "common.h"
#include "tos_errors.h"
#include "sysvars.h"

typedef struct memblock {
	struct memblock* next;
	emuptr32_t start;
	int32_t size;
} memblock_t;

memblock_t freeMem;
memblock_t usedMem;
memblock_t* rover;

void InitMemory()
{
	m68k_write_memory_32(_membot, 0x10000);
	m68k_write_memory_32(_memtop, memory_sz-1);
	m68k_write_memory_32(phystop, memory_sz);
	usedMem.next = NULL;
	usedMem.start = -1;
	usedMem.size = -1;
	memblock_t* firstBlock = malloc(sizeof(memblock_t));
	firstBlock->start = 0x10000;
	firstBlock->size = memory_sz-firstBlock->start;
	firstBlock->next = NULL;
	freeMem.next = firstBlock;
	freeMem.start = -1;
	freeMem.size = -1;
	rover = &freeMem;
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
emuptr32_t Malloc ( int32_t size )
{
	uint32_t max = 0;
	if (size != -1)
	{
		size = (size + 3) & ~3;
	}

	if (freeMem.next == NULL)
		return 0; // Out of memory

	memblock_t* prev = rover;
	memblock_t* current = prev->next;
	do {
		if (current == NULL) // wrap around
		{
			prev = &freeMem;
			current = prev->next;
		}
		if (size == -1)
		{
			if (max < current->size)
				max = current->size;
		}
		else if(current->size >= size)
		{
			if (current->size == size)
			{
				prev->next = current->next;
			}
			else
			{
				memblock_t* new = malloc(sizeof(memblock_t));
				prev->next = new;
				new->next = current->next;
				new->start = current->start + size;
				new->size = current->size - size;
				current->size = size;
			}
			rover = (prev == &freeMem?prev->next:prev);
			current->next = usedMem.next;
			usedMem.next = current;
			return current->start;
		}
		current = (prev=current)->next;
	} while(prev != rover);

	return max;
}

// Returns a free block back to the free list
static void freeBlock(memblock_t* freeme)
{
	memblock_t* prev=NULL;
	memblock_t* current;
	for(current = freeMem.next; current; current = (prev=current)->next)
		if(freeme->start <= current->start)
			break;
	freeme->next = current;
	if(prev)
	{
		prev->next=freeme;
	}
	else
	{
		freeMem.next=freeme;
	}

	if(!rover)
		rover = freeme;

	if(current && freeme->start + freeme->size == current->start)
	{
		freeme->size += current->size;
		freeme->next = current->next;
		if (rover == current)
			rover = freeme;
		free(current);
	}

	if(prev && prev->start + prev->size == freeme->start)
	{
		prev->size += freeme->size;
		prev->next = freeme->next;
		if (rover == freeme)
			rover = prev;
		free(freeme);
	}

#if 0
	printf("Free blocks:\n");
	for(memblock_t* c=freeMem.next; c; c =c->next)
	{
		printf("\t%06x - %06x (sz %x)\n", c->start, c->start+c->size-1,c->size);
	}
#endif
}

/**
 * Mfree - 73
 *
 * The GEMDOS routine Mfree releases a block of memory previously reserved
 * with Malloc.
 *
 * The parameter block contains the start address of the memory block to be
 * released.
 *
 * Note: In almost all GEMDOS versions no check is made whether the block to
 * be released really belongs to the relevant process. Hence particular care
 * is needed, specially in multitasking systems.
 *
 * int32_t Mfree ( void *block )
 */
int32_t Mfree ( emuptr32_t block )
{
	memblock_t* prev = &usedMem;
	for(memblock_t* current=usedMem.next; current; current = (prev=current)->next)
	{
		if(current->start == block)
		{
			prev->next = current->next;
			freeBlock(current);
			return TOS_E_OK;
		}
	}
	return TOS_EIMBA;
}

/**
 * Mshrink - 74
 *
 * The GEMDOS routine Mshrink reduces or enlarges an already reserved memory
 * block in the GEMDOS free list. The following apply: Parameter Meaning
 *
 * int32_t Mshrink ( void *block, int32_t newsiz )
 */
int32_t Mshrink ( emuptr32_t block, int32_t newsiz )
{
	if(newsiz == 0)
		return Mfree(block);
	newsiz = (newsiz + 3) & ~3;
	for(memblock_t* current=usedMem.next; current; current = current->next)
	{
		if(current->start == block)
		{
			if(current->size < newsiz)
			{
				return TOS_EGSBF;
			}
			memblock_t* new = malloc(sizeof(memblock_t));
			new->start = current->start+newsiz;
			new->size = current->size-newsiz;
			current->size=newsiz;
			freeBlock(new);
			return newsiz;
		}
	}
	return TOS_EIMBA;
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
