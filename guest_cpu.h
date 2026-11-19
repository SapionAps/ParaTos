#pragma once
#include <stdint.h>
#include "mushashi/m68k.h"

/* A 68k PRG receives its basepage at 4(sp) and a zero return address.
 * A0 is clear for normal PRG startup, as opposed to accessory startup. */
static inline void guest_cpu_start(uint32_t bp, uint32_t entry, uint32_t high,
	uint32_t supervisor)
{
	uint32_t stack = (high & ~3u) - 8;
	m68k_write_memory_32(stack, 0);
	m68k_write_memory_32(stack + 4, bp);
	m68k_set_reg(M68K_REG_SR, 0x0300);
	m68k_set_reg(M68K_REG_ISP, supervisor);
	m68k_set_reg(M68K_REG_SP, stack);
	m68k_set_reg(M68K_REG_A0, 0);
	m68k_set_reg(M68K_REG_PC, entry);
}
