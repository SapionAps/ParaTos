#include "native.h"

/* These are pTOS trap numbers, not Linux syscall numbers. The GEMDOS ABI
 * carries six scalar arguments on both ARM and x32. */

int32_t ptos_call(uint16_t fn, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
	return ptos_native_call6(fn, a, b, c, d, 0, 0);
}

int32_t ptos_bios_call(uint16_t fn, int16_t device, int16_t value)
{
	return ptos_native_bios_call(fn, device, value);
}

int32_t ptos_map(uint32_t address, uint32_t length)
{
	/* PRIVATE | ANONYMOUS | FIXED_NOREPLACE, RW (guest code is CPU data). */
	return ptos_native_call6(GEMDOS_OP_MMAP, address, length, 3, 0x100022, -1, 0);
}

int32_t ptos_unmap(uint32_t address, uint32_t length)
{
	return ptos_call(GEMDOS_OP_MUNMAP, address, length, 0, 0);
}
