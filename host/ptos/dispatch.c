#include <string.h>
#ifdef PARATOS_TRACE
#include <stdio.h>
#endif
#include "m68k.h"
#include "host.h"
#include "memory.h"
#include "guest.h"
#include "filesystem.h"
#include "vdi.h"

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
		uint32_t jar = m68k_read_memory_32(0x5a0), at = jar;
		for (unsigned int i = 0; i < 4096 && at <= UINT32_MAX - 8; i++, at += 8)
		{
			uint32_t tag = m68k_read_memory_32(at);
			if (!tag)
			{
				uint32_t capacity = m68k_read_memory_32(at + 4);
				if (a > 0xffff || a > capacity || a > 4096) return -1;
				value = a ? m68k_read_memory_32(jar + (a - 1) * 8) : capacity;
			}
			else if (a > 0xffff && tag == a) value = m68k_read_memory_32(at + 4);
			else continue;
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
	case GEMDOS_OP_SSYNC:
	case GEMDOS_OP_SVERSION:
	case GEMDOS_OP_PGETPID:
	case GEMDOS_OP_PGETPPID:
	case GEMDOS_OP_PGETPGRP:
	case GEMDOS_OP_PGETEUID:
	case GEMDOS_OP_PGETEGID:
	case GEMDOS_OP_PGETAUID:
	case GEMDOS_OP_PGETUID:
	case GEMDOS_OP_PGETGID:
		r = ptos_call(fn, 0, 0, 0, 0); break;
	case GEMDOS_OP_PNICE:
	case GEMDOS_OP_PUMASK:
	case GEMDOS_OP_PSETAUID:
	case GEMDOS_OP_CCONOUT:
	case GEMDOS_OP_CAUXOUT:
	case GEMDOS_OP_CPRNOUT:
	case GEMDOS_OP_CRAWIO:
	case GEMDOS_OP_DSETDRV:
	case GEMDOS_OP_TSETDATE:
	case GEMDOS_OP_TSETTIME:
		r = ptos_call(fn, (int16_t)word(sp, 2), 0, 0, 0); break;
	case GEMDOS_OP_FCLOSE: r = ptos_file_close(word(sp, 2)); break;
	case GEMDOS_OP_FDUP: r = ptos_file_dup(word(sp, 2)); break;
	case GEMDOS_OP_CCONWS: r = ptos_call(fn, (intptr_t)ptos_guest_string(lng(sp, 2), 32766), 0, 0, 0); break;
	case GEMDOS_OP_CCONRS: {
		uint32_t buf = lng(sp, 2);
		r = ptos_call(fn, (intptr_t)ptos_guest_buffer(buf, 2 + m68k_read_memory_8(buf)), 0, 0, 0); break;
	}
	case GEMDOS_OP_FSETDTA:
		ptos_guest_buffer(lng(sp, 2), 44);
		m68k_write_memory_32(ptos_process + BP_DTA, lng(sp, 2)); r = 0; break;
	case GEMDOS_OP_FGETDTA: r = m68k_read_memory_32(ptos_process + BP_DTA); break;
	case GEMDOS_OP_SUPER: r = ptos_super(lng(sp, 2)); break;
	case GEMDOS_OP_DCREATE:
	case GEMDOS_OP_DDELETE:
	case GEMDOS_OP_DSETPATH:
	case GEMDOS_OP_DCHROOT:
	case GEMDOS_OP_FDELETE:
		r = ptos_call(fn, path(lng(sp, 2), p1), 0, 0, 0); break;
	case GEMDOS_OP_FCREATE:
	case GEMDOS_OP_FOPEN:
		path(lng(sp, 2), p1);
		r = ptos_file_open(fn, p1, word(sp, 6)); break;
	case GEMDOS_OP_FREAD:
	case GEMDOS_OP_FWRITE: {
		int32_t count = (int32_t)lng(sp, 4);
		if (count < 0) { r = PTOS_EINVAL; break; }
		int32_t handle = ptos_file_handle(word(sp, 2));
		if (handle == PTOS_EIHNDL) { r = handle; break; }
		r = ptos_call(fn, handle, count,
			(intptr_t)ptos_guest_buffer(lng(sp, 8), (uint32_t)count), 0); break;
	}
	case GEMDOS_OP_FSEEK: {
		int32_t handle = ptos_file_handle(word(sp, 6));
		r = handle == PTOS_EIHNDL ? handle : ptos_call(fn, (int32_t)lng(sp, 2), handle, word(sp, 8), 0); break;
	}
	case GEMDOS_OP_PGETPRIORITY:
		r = ptos_call(fn, (int16_t)word(sp, 2), (int16_t)word(sp, 4), 0, 0); break;
	case GEMDOS_OP_PSETPRIORITY:
		r = ptos_call(fn, (int16_t)word(sp, 2), (int16_t)word(sp, 4), (int16_t)word(sp, 6), 0); break;
	case GEMDOS_OP_PRUSAGE: {
		uint32_t out = lng(sp, 2), usage[8] = {0};
		ptos_guest_buffer(out, sizeof(usage));
		r = ptos_call(fn, (intptr_t)usage, 0, 0, 0);
		if (r >= 0)
			for (unsigned int i = 0; i < 8; i++) m68k_write_memory_32(out + i * 4, usage[i]);
		break;
	}
	case GEMDOS_OP_DFREE: r = ptos_disk_free(lng(sp, 2), word(sp, 6)); break;
	case GEMDOS_OP_FSFIRST: r = ptos_search(fn, (const char *)path(lng(sp, 2), p1), word(sp, 6)); break;
	case GEMDOS_OP_FSNEXT: r = ptos_search(fn, NULL, 0); break;
	case GEMDOS_OP_FXATTR: r = ptos_xattr(word(sp, 2), (const char *)path(lng(sp, 4), p1), lng(sp, 8)); break;
	case GEMDOS_OP_DPATHCONF: r = ptos_call(fn, path(lng(sp, 2), p1), (int16_t)word(sp, 6), 0, 0); break;
	case GEMDOS_OP_FCHMOD: r = ptos_call(fn, path(lng(sp, 2), p1), word(sp, 6), 0, 0); break;
	case GEMDOS_OP_FCHOWN: r = ptos_call(fn, path(lng(sp, 2), p1), (int16_t)word(sp, 6), (int16_t)word(sp, 8), 0); break;
	case GEMDOS_OP_DLOCK: r = ptos_call(fn, (int16_t)word(sp, 2), (int16_t)word(sp, 4), 0, 0); break;
	case GEMDOS_OP_DGETCWD: {
		int16_t size = word(sp, 8);
		char scratch = 0;
		void *buffer = size > 0 ? ptos_guest_buffer(lng(sp, 2), size) : &scratch;
		r = ptos_call(fn, (intptr_t)buffer, (int16_t)word(sp, 6), size, 0); break;
	}
	case GEMDOS_OP_FLINK:
	case GEMDOS_OP_FSYMLINK:
		r = ptos_call(fn, path(lng(sp, 2), p1), path(lng(sp, 6), p2), 0, 0); break;
	case GEMDOS_OP_FREADLINK: {
		int16_t size = word(sp, 2);
		char scratch = 0;
		void *buffer = size > 0 ? ptos_guest_buffer(lng(sp, 4), size) : &scratch;
		r = ptos_call(fn, size, (intptr_t)buffer, path(lng(sp, 8), p1), 0); break;
	}
	case GEMDOS_OP_FATTRIB: r = ptos_call(fn, path(lng(sp, 2), p1), word(sp, 6), word(sp, 8), 0); break;
	case GEMDOS_OP_MXALLOC: { /* No TT-RAM in this initial guest layout. */
		uint16_t mode = word(sp, 6);
		r = mode > 3 ? PTOS_EINVAL : mode == 1 ? PTOS_ENSMEM : ptos_alloc((int32_t)lng(sp, 2));
		break;
	}
	case GEMDOS_OP_MALLOC: r = ptos_alloc((int32_t)lng(sp, 2)); break;
	case GEMDOS_OP_FFORCE: {
		r = ptos_file_force(word(sp, 2), word(sp, 4)); break;
	}
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
		int32_t handle = ptos_file_handle(word(sp, 6));
		if (handle == PTOS_EIHNDL) { r = handle; break; }
		uint16_t date[2];
		if (writing) { date[0] = m68k_read_memory_16(at); date[1] = m68k_read_memory_16(at + 2); }
		r = ptos_call(fn, (intptr_t)date, handle, writing, 0);
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
		if (fn == BIOS_OP_BCONSTAT || fn == BIOS_OP_BCONIN || fn == BIOS_OP_BCOSTAT)
			r = ptos_bios_call(fn, device, 0);
		if (fn == BIOS_OP_BCONOUT) r = ptos_bios_call(fn, device, word(sp, 4));
	}
	m68k_set_reg(M68K_REG_D0, r);
}
void dispatch_gem_trap(void)
{
	int32_t result = PTOS_ENOSYS;
	if (m68k_get_reg(NULL, M68K_REG_D0) == VDI_TRAP)
		result = ptos_vdi(m68k_get_reg(NULL, M68K_REG_D1));
	m68k_set_reg(M68K_REG_D0, (uint32_t)result);
}
void dispatch_line_a(void) { ptos_fault(m68k_get_reg(NULL, M68K_REG_PC), "Line-A is not supported"); }
void dispatch_line_f(void) { ptos_fault(m68k_get_reg(NULL, M68K_REG_PC), "FPU is not supported"); }
