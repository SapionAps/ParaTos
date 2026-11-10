#pragma once
#include "host.h"

/* Implemented by the selected architecture; all native pTOS ports here use
 * 32-bit user pointers/scalars, independently of the kernel's word size. */
int32_t ptos_native_call6(uint16_t fn, intptr_t a, intptr_t b, intptr_t c,
	intptr_t d, intptr_t e, intptr_t f);
int32_t ptos_native_bios_call(uint16_t fn, int16_t device, int16_t value);
