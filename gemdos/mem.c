#include "common.h"
#include "tos_errors.h"

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
	NOT_IMPLEMENTED(GDOS, Maddalt, 20);
	return -TOS_ENOSYS;
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
emuptr32_t Malloc ( int32_t number )
{
	NOT_IMPLEMENTED(GDOS, Malloc, 72);
	return -TOS_ENOSYS;
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
	NOT_IMPLEMENTED(GDOS, Mfree, 73);
	return -TOS_ENOSYS;
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
	NOT_IMPLEMENTED(GDOS, Mshrink, 74);
	return -TOS_ENOSYS;
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
	NOT_IMPLEMENTED(GDOS, Mxalloc, 68);
	return -TOS_ENOSYS;
}

