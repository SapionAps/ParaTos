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
	if (mode >= SSYSTEM_OP_GETLVAL && mode <= SSYSTEM_OP_GETBVAL)
	{
		int32_t result = ptos_low_read(a, mode == SSYSTEM_OP_GETLVAL ? 4 : mode == SSYSTEM_OP_GETWVAL ? 2 : 1, &value);
		return result < 0 ? result : (int32_t)value;
	}
	if (mode >= SSYSTEM_OP_SETLVAL && mode <= SSYSTEM_OP_SETBVAL)
		return ptos_low_write(a, mode == SSYSTEM_OP_SETLVAL ? 4 : mode == SSYSTEM_OP_SETWVAL ? 2 : 1, b);
	if (mode == SSYSTEM_OP_GETCOOKIE) /* guest cookie jar, never native pointers or CPU identity */
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
	if (mode == SSYSTEM_OP_INQUIRE)
	{
		uint16_t query = a;
		return query == SSYSTEM_OP_OSNAME || query == SSYSTEM_OP_OSXNAME ||
			query == SSYSTEM_OP_OSVERSION || query == SSYSTEM_OP_OSBUILDDATE ||
			query == SSYSTEM_OP_OSFEATURES || query == SSYSTEM_OP_GETCOOKIE ||
			(query >= SSYSTEM_OP_GETLVAL && query <= SSYSTEM_OP_SETBVAL) ||
			query == SSYSTEM_OP_INQUIRE ? 0 : PTOS_ENOSYS;
	}
	/* The guest has a flat TOS address space. The native kernel's MMU,
	 * demand-paging and native-ELF features are not guest capabilities. */
	if (mode == SSYSTEM_OP_OSFEATURES) return 0;
	/* Only pointer-free queries with guest-compatible scalar answers. */
	if (mode == SSYSTEM_OP_OSNAME || mode == SSYSTEM_OP_OSXNAME ||
		mode == SSYSTEM_OP_OSVERSION || mode == SSYSTEM_OP_OSBUILDDATE)
		return ptos_call(GEMDOS_OP_SSYSTEM, mode, a, b, 0);
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
	case GEMDOS_OP_PTERM0: ptos_terminate(0); return;
	case GEMDOS_OP_PTERM: ptos_terminate(word(sp, 2)); return;
	case GEMDOS_OP_CCONIN:
	case GEMDOS_OP_CAUXIN:
	case GEMDOS_OP_CRAWCIN:
	case GEMDOS_OP_CNECIN:
	case GEMDOS_OP_CCONIS:
	case GEMDOS_OP_CCONOS:
	case GEMDOS_OP_CPRNOS:
	case GEMDOS_OP_CAUXIS:
	case GEMDOS_OP_CAUXOS:
	case GEMDOS_OP_DGETDRV:
	case GEMDOS_OP_TGETDATE:
	case GEMDOS_OP_TGETTIME:
	case GEMDOS_OP_SVERSION:
	case GEMDOS_OP_PGETPID:
	case GEMDOS_OP_PGETPPID:
	case GEMDOS_OP_PGETPGRP:
	case GEMDOS_OP_PGETUID:
	case GEMDOS_OP_PGETGID:
		r = ptos_call(fn, 0, 0, 0, 0); break;
	case GEMDOS_OP_CCONOUT:
	case GEMDOS_OP_CAUXOUT:
	case GEMDOS_OP_CPRNOUT:
	case GEMDOS_OP_CRAWIO:
	case GEMDOS_OP_DSETDRV:
	case GEMDOS_OP_TSETDATE:
	case GEMDOS_OP_TSETTIME:
	case GEMDOS_OP_FCLOSE:
	case GEMDOS_OP_FDUP:
		r = ptos_call(fn, (int16_t)word(sp, 2), 0, 0, 0); break;
	case GEMDOS_OP_CCONWS: r = ptos_call(fn, (intptr_t)ptos_guest_string(lng(sp, 2), 32766), 0, 0, 0); break;
	case GEMDOS_OP_CCONRS: {
		uint32_t buf = lng(sp, 2);
		r = ptos_call(fn, (intptr_t)ptos_guest_buffer(buf, 2 + m68k_read_memory_8(buf)), 0, 0, 0); break;
	}
	case GEMDOS_OP_FSETDTA:
		dta = lng(sp, 2); ptos_guest_buffer(dta, 44);
		m68k_write_memory_32(ptos_process + BP_DTA, dta); r = 0; break;
	case GEMDOS_OP_FGETDTA: r = m68k_read_memory_32(ptos_process + BP_DTA); break;
	case GEMDOS_OP_SUPER: r = ptos_super(lng(sp, 2)); break;
	case GEMDOS_OP_DCREATE:
	case GEMDOS_OP_DDELETE:
	case GEMDOS_OP_DSETPATH:
	case GEMDOS_OP_FDELETE:
		r = ptos_call(fn, path(lng(sp, 2), p1), 0, 0, 0); break;
	case GEMDOS_OP_FCREATE:
	case GEMDOS_OP_FOPEN:
		r = ptos_call(fn, path(lng(sp, 2), p1), word(sp, 6), 0, 0); break;
	case GEMDOS_OP_FREAD:
	case GEMDOS_OP_FWRITE: {
		int32_t count = (int32_t)lng(sp, 4);
		if (count < 0) { r = PTOS_EINVAL; break; }
		r = ptos_call(fn, (int16_t)word(sp, 2), count,
			(intptr_t)ptos_guest_buffer(lng(sp, 8), (uint32_t)count), 0); break;
	}
	case GEMDOS_OP_FSEEK: r = ptos_call(fn, (int32_t)lng(sp, 2), (int16_t)word(sp, 6), word(sp, 8), 0); break;
	case GEMDOS_OP_FATTRIB: r = ptos_call(fn, path(lng(sp, 2), p1), word(sp, 6), word(sp, 8), 0); break;
	case GEMDOS_OP_MXALLOC: { /* No TT-RAM in this initial guest layout. */
		uint16_t mode = word(sp, 6);
		r = mode > 3 ? PTOS_EINVAL : mode == 1 ? PTOS_ENSMEM : ptos_alloc((int32_t)lng(sp, 2));
		break;
	}
	case GEMDOS_OP_MALLOC: r = ptos_alloc((int32_t)lng(sp, 2)); break;
	case GEMDOS_OP_FFORCE: r = ptos_call(fn, (int16_t)word(sp, 2), (int16_t)word(sp, 4), 0, 0); break;
	case GEMDOS_OP_DGETPATH: r = ptos_call(fn, (intptr_t)ptos_guest_buffer(lng(sp, 2), 128), word(sp, 6), 0, 0); break;
	case GEMDOS_OP_MFREE: r = ptos_free(lng(sp, 2)); break;
	case GEMDOS_OP_MSHRINK: r = ptos_shrink(lng(sp, 4), (int32_t)lng(sp, 8)); break;
	case GEMDOS_OP_PEXEC:
		r = ptos_exec(word(sp, 2), lng(sp, 4), lng(sp, 8), lng(sp, 12));
		if (r == 0 && ((word(sp, 2) & 0x7fff) == PTOS_EXEC_GO ||
			(word(sp, 2) & 0x7fff) == PTOS_EXEC_GO_FREE)) return;
		break;
	case GEMDOS_OP_FRENAME: r = ptos_call(fn, 0, path(lng(sp, 4), p1), path(lng(sp, 8), p2), 0); break;
	case GEMDOS_OP_FDATIME: {
		uint32_t at = lng(sp, 2); int writing = word(sp, 8);
		uint16_t date[2];
		if (writing) { date[0] = m68k_read_memory_16(at); date[1] = m68k_read_memory_16(at + 2); }
		r = ptos_call(fn, (intptr_t)date, (int16_t)word(sp, 6), writing, 0);
		if (r >= 0 && !writing) { m68k_write_memory_16(at, date[0]); m68k_write_memory_16(at + 2, date[1]); }
		break;
	}
	case GEMDOS_OP_SSYSTEM: r = guest_ssystem(word(sp, 2), lng(sp, 4), lng(sp, 8)); break;
	case GEMDOS_OP_SYSCONF: {
		int16_t query = word(sp, 2);
		/* Report guest RAM, not the host machine's physical page counts. */
		if (query == SYSCONF_QUERY_PHYS_PAGES) r = (PTOS_RAM_END - PTOS_RAM_START) / PTOS_PAGE_BYTES;
		else if (query == SYSCONF_QUERY_AVPHYS_PAGES) r = PTOS_ENOSYS;
		else r = ptos_call(fn, query, 0, 0, 0);
		break;
	}
	default: break; /* unimplemented ABIs must not forward guest structures blindly */
	}
	m68k_set_reg(M68K_REG_D0, (uint32_t)r);
}

void dispatch_xbios_trap(void)
{
	if (ptos_super_return()) return;
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
	if (word(sp, 0) == XBIOS_OP_SUPEXEC)
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
	if (fn == BIOS_OP_SETEXC) /* includes the traditional cookie-jar inquiry */
	{
		uint32_t at = (uint32_t)device * 4;
		if (at + 4 <= PTOS_RAM_START)
		{
			r = m68k_read_memory_32(at);
			uint32_t replacement = lng(sp, 4);
			if (replacement != UINT32_MAX) m68k_write_memory_32(at, replacement);
		}
	}
	else if (device <= BIOS_DEVICE_CONSOLE)
	{
		if (fn == BIOS_OP_BCONSTAT)
			r = ptos_call(device == BIOS_DEVICE_CONSOLE ? GEMDOS_OP_CCONIS :
				device == BIOS_DEVICE_AUX ? GEMDOS_OP_CAUXIS : GEMDOS_OP_CPRNOS, 0, 0, 0, 0);
		if (fn == BIOS_OP_BCONIN && device != BIOS_DEVICE_PRINTER)
			r = ptos_call(device == BIOS_DEVICE_CONSOLE ? GEMDOS_OP_CRAWCIN : GEMDOS_OP_CAUXIN, 0, 0, 0, 0);
		if (fn == BIOS_OP_BCONOUT)
			r = ptos_call(device == BIOS_DEVICE_CONSOLE ? GEMDOS_OP_CCONOUT :
				device == BIOS_DEVICE_AUX ? GEMDOS_OP_CAUXOUT : GEMDOS_OP_CPRNOUT, word(sp, 4), 0, 0, 0);
		if (fn == BIOS_OP_BCOSTAT)
			r = ptos_call(device == BIOS_DEVICE_CONSOLE ? GEMDOS_OP_CCONOS :
				device == BIOS_DEVICE_AUX ? GEMDOS_OP_CAUXOS : GEMDOS_OP_CPRNOS, 0, 0, 0, 0);
	}
	m68k_set_reg(M68K_REG_D0, r);
}
void dispatch_gem_trap(void) { m68k_set_reg(M68K_REG_D0, PTOS_ENOSYS); }
void dispatch_line_a(void) { ptos_fault(m68k_get_reg(NULL, M68K_REG_PC), "Line-A is not supported"); }
void dispatch_line_f(void) { ptos_fault(m68k_get_reg(NULL, M68K_REG_PC), "FPU is not supported"); }
