#include <string.h>
#ifdef PARATOS_TRACE
#include <stdio.h>
#endif
#include "m68k.h"
#include "host.h"
#include "memory.h"
#include "guest.h"

static uint32_t dta;
static uint32_t word(uint32_t sp, unsigned int offset) { return m68k_read_memory_16(sp + offset); }
static uint32_t lng(uint32_t sp, unsigned int offset) { return m68k_read_memory_32(sp + offset); }
static intptr_t path(uint32_t address, char *buffer)
{
	const char *s = ptos_guest_string(address, 1024);
	size_t n = strlen(s);
	for (size_t i = 0; i <= n; i++) buffer[i] = s[i] == '/' ? '\\' : s[i];
	return (intptr_t)buffer;
}

static int32_t guest_ssystem(uint16_t mode, uint32_t a, uint32_t b)
{
	uint32_t value;
	if (mode >= 0x0a && mode <= 0x0c)
	{
		int32_t result = ptos_low_read(a, mode == 0x0a ? 4 : mode == 0x0b ? 2 : 1, &value);
		return result < 0 ? result : (int32_t)value;
	}
	if (mode >= 0x0d && mode <= 0x0f) return ptos_low_write(a, mode == 0x0d ? 4 : mode == 0x0e ? 2 : 1, b);
	if (mode == 8) /* guest cookie jar, never native pointers or CPU identity */
	{
		uint32_t at = m68k_read_memory_32(0x5a0);
		for (unsigned int i = 0; i < 4096 && at <= UINT32_MAX - 8; i++, at += 8)
		{
			uint32_t tag = m68k_read_memory_32(at);
			if (!tag) break;
			if (tag != a) continue;
			value = m68k_read_memory_32(at + 4);
			if (b) { m68k_write_memory_32(b, value); return 0; }
			return value;
		}
		return -1;
	}
	if (mode == 0xffff)
	{
		uint16_t query = a;
		return query == 0 || query == 1 || query == 2 || query == 4 || query == 7 ||
			query == 8 || (query >= 0xa && query <= 0xf) || query == 0xffff ? 0 : PTOS_ENOSYS;
	}
	/* Only pointer-free queries with guest-compatible scalar answers. */
	if (mode == 0 || mode == 1 || mode == 2 || mode == 4 || mode == 7)
		return ptos_call(0x154, mode, a, b, 0);
	return PTOS_ENOSYS;
}

void dispatch_gemdos_trap(void)
{
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
	uint16_t fn = word(sp, 0);
#ifdef PARATOS_TRACE
	fprintf(stderr, "guest GEMDOS %04x\n", fn);
#endif
	int32_t r = PTOS_ENOSYS;
	char p1[1024], p2[1024];
	switch (fn)
	{
	case 0: ptos_terminate(0); return;
	case 0x4c: ptos_terminate(word(sp, 2)); return;
	case 1: case 3: case 7: case 8: case 0xb: case 0x10: case 0x11:
	case 0x12: case 0x13: case 0x19: case 0x2a: case 0x2c: case 0x30:
	case 0x10b: case 0x10c: case 0x10d: case 0x10f: case 0x114:
		r = ptos_call(fn, 0, 0, 0, 0); break;
	case 2: case 4: case 5: case 6: case 0xe: case 0x2b: case 0x2d:
	case 0x3e: case 0x45:
		r = ptos_call(fn, (int16_t)word(sp, 2), 0, 0, 0); break;
	case 9: r = ptos_call(fn, (intptr_t)ptos_guest_string(lng(sp, 2), 32766), 0, 0, 0); break;
	case 0x0a: {
		uint32_t buf = lng(sp, 2);
		r = ptos_call(fn, (intptr_t)ptos_guest_buffer(buf, 2 + m68k_read_memory_8(buf)), 0, 0, 0); break;
	}
	case 0x1a:
		dta = lng(sp, 2); ptos_guest_buffer(dta, 44);
		m68k_write_memory_32(ptos_process + BP_DTA, dta); r = 0; break;
	case 0x2f: r = m68k_read_memory_32(ptos_process + BP_DTA); break;
	case 0x20: r = ptos_super(lng(sp, 2)); break;
	case 0x39: case 0x3a: case 0x3b: case 0x41:
		r = ptos_call(fn, path(lng(sp, 2), p1), 0, 0, 0); break;
	case 0x3c: case 0x3d:
		r = ptos_call(fn, path(lng(sp, 2), p1), word(sp, 6), 0, 0); break;
	case 0x3f: case 0x40: {
		int32_t count = (int32_t)lng(sp, 4);
		if (count < 0) { r = PTOS_EINVAL; break; }
		r = ptos_call(fn, (int16_t)word(sp, 2), count,
			(intptr_t)ptos_guest_buffer(lng(sp, 8), (uint32_t)count), 0); break;
	}
	case 0x42: r = ptos_call(fn, (int32_t)lng(sp, 2), (int16_t)word(sp, 6), word(sp, 8), 0); break;
	case 0x43: r = ptos_call(fn, path(lng(sp, 2), p1), word(sp, 6), word(sp, 8), 0); break;
	case 0x44: { /* No TT-RAM in this initial guest layout. */
		uint16_t mode = word(sp, 6);
		r = mode > 3 ? PTOS_EINVAL : mode == 1 ? PTOS_ENSMEM : ptos_alloc((int32_t)lng(sp, 2));
		break;
	}
	case 0x48: r = ptos_alloc((int32_t)lng(sp, 2)); break;
	case 0x46: r = ptos_call(fn, (int16_t)word(sp, 2), (int16_t)word(sp, 4), 0, 0); break;
	case 0x47: r = ptos_call(fn, (intptr_t)ptos_guest_buffer(lng(sp, 2), 128), word(sp, 6), 0, 0); break;
	case 0x49: r = ptos_free(lng(sp, 2)); break;
	case 0x4a: r = ptos_shrink(lng(sp, 4), (int32_t)lng(sp, 8)); break;
	case 0x4b:
		r = ptos_exec(word(sp, 2), lng(sp, 4), lng(sp, 8), lng(sp, 12));
		if (r == 0 && ((word(sp, 2) & 0x7fff) == 4 || (word(sp, 2) & 0x7fff) == 6)) return;
		break;
	case 0x56: r = ptos_call(fn, 0, path(lng(sp, 4), p1), path(lng(sp, 8), p2), 0); break;
	case 0x57: {
		uint32_t at = lng(sp, 2); int writing = word(sp, 8);
		uint16_t date[2];
		if (writing) { date[0] = m68k_read_memory_16(at); date[1] = m68k_read_memory_16(at + 2); }
		r = ptos_call(fn, (intptr_t)date, (int16_t)word(sp, 6), writing, 0);
		if (r >= 0 && !writing) { m68k_write_memory_16(at, date[0]); m68k_write_memory_16(at + 2, date[1]); }
		break;
	}
	case 0x154: r = guest_ssystem(word(sp, 2), lng(sp, 4), lng(sp, 8)); break;
	default: break; /* unimplemented ABIs must not forward guest structures blindly */
	}
	m68k_set_reg(M68K_REG_D0, (uint32_t)r);
}

void dispatch_xbios_trap(void)
{
	if (ptos_super_return()) return;
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
	if (word(sp, 0) == 38)
	{
		int32_t r = ptos_supexec(lng(sp, 2));
		if (r < 0) m68k_set_reg(M68K_REG_D0, r);
		return;
	}
	m68k_set_reg(M68K_REG_D0, PTOS_ENOSYS);
}

void dispatch_bios_trap(void)
{
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
	uint16_t fn = word(sp, 0), device = word(sp, 2);
	int32_t r = PTOS_ENOSYS;
	if (fn == 5) /* Setexc, including the traditional cookie-jar inquiry */
	{
		uint32_t at = (uint32_t)device * 4;
		if (at + 4 <= PTOS_RAM_START)
		{
			r = m68k_read_memory_32(at);
			uint32_t replacement = lng(sp, 4);
			if (replacement != UINT32_MAX) m68k_write_memory_32(at, replacement);
		}
	}
	else if (device <= 2)
	{
		if (fn == 1) r = ptos_call(device == 2 ? 0xb : device == 1 ? 0x12 : 0x11, 0, 0, 0, 0);
		if (fn == 2 && device) r = ptos_call(device == 2 ? 7 : 3, 0, 0, 0, 0);
		if (fn == 3) r = ptos_call(device == 2 ? 2 : device == 1 ? 4 : 5, word(sp, 4), 0, 0, 0);
		if (fn == 8) r = ptos_call(device == 2 ? 0x10 : device == 1 ? 0x13 : 0x11, 0, 0, 0, 0);
	}
	m68k_set_reg(M68K_REG_D0, r);
}
void dispatch_gem_trap(void) { m68k_set_reg(M68K_REG_D0, PTOS_ENOSYS); }
void dispatch_line_a(void) { ptos_fault(m68k_get_reg(NULL, M68K_REG_PC), "Line-A is not supported"); }
void dispatch_line_f(void) { ptos_fault(m68k_get_reg(NULL, M68K_REG_PC), "FPU is not supported"); }
