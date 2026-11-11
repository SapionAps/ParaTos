#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "m68k.h"
#include "host.h"
#include "memory.h"
#include "guest.h"
#include "filesystem.h"
#include "filesystem_contract.h"

void dispatch_gemdos_trap(void);
#define DEFAULT_DTA ((intptr_t)0x10000000)
static intptr_t native_dta = DEFAULT_DTA;
static int32_t error;
static int fail_restore;
static struct { intptr_t address; unsigned int position, stamp; } cursors[8];
static unsigned int cursor_clock;
/* Pluggable filesystem cursors survive an early Fsfirst path error even
 * after the protected DTA cache has forgotten the same native address. */
static intptr_t filesystem_cursors[16];

static unsigned int filesystem_searches(void)
{
	unsigned int count = 0;
	for (unsigned int i = 0; i < 16; i++) count += filesystem_cursors[i] != 0;
	return count;
}

static void forget_filesystem_cursor(intptr_t address)
{
	for (unsigned int i = 0; i < 16; i++)
		if (filesystem_cursors[i] == address) filesystem_cursors[i] = 0;
}

static int filesystem_cursor_exists(intptr_t address)
{
	for (unsigned int i = 0; i < 16; i++)
		if (filesystem_cursors[i] == address) return 1;
	return 0;
}

static unsigned int native_searches(void)
{
	unsigned int count = 0;
	for (unsigned int i = 0; i < 8; i++) count += cursors[i].address != 0;
	return count;
}

static void forget_cursor(intptr_t address)
{
	for (unsigned int i = 0; i < 8; i++)
		if (cursors[i].address == address) cursors[i].address = 0;
}
static uint16_t last_function;
static intptr_t arguments[4];
static char first_path[1024], second_path[1024];

int filesystem_test_native(uint16_t fn, intptr_t a, intptr_t b, intptr_t c, intptr_t d, int32_t *result)
{
	if (fn == GEMDOS_OP_FGETDTA) { *result = (int32_t)native_dta; return 1; }
	if (fn == GEMDOS_OP_FSETDTA) {
		if (a == DEFAULT_DTA && fail_restore) { *result = -36; return 1; }
		native_dta = a; *result = 0; return 1;
	}
	switch (fn)
	{
	case GEMDOS_OP_FSFIRST:
	case GEMDOS_OP_FSNEXT: {
		last_function = fn;
		assert(native_dta != DEFAULT_DTA);
		ptos_native_dta *value = (ptos_native_dta *)native_dta;
		if (fn == GEMDOS_OP_FSFIRST)
		{
			forget_cursor(native_dta); /* native Fsfirst always forgets first */
			if (!strcmp((const char *)a, "?:\\")) { *result = error ? error : -46; return 1; }
			assert(!strcmp((const char *)a, "C:\\SEARCH\\*.TXT"));
		}
		if (error)
		{
			if (fn == GEMDOS_OP_FSNEXT) forget_filesystem_cursor(native_dta);
			memset(value, 0xcc, sizeof(*value)); *result = error; return 1;
		}
		if (fn == GEMDOS_OP_FSFIRST)
		{
			forget_filesystem_cursor(native_dta);
			unsigned int slot;
			for (slot = 0; slot < 16; slot++) if (!filesystem_cursors[slot]) break;
			if (slot == 16) { *result = -35; return 1; }
			filesystem_cursors[slot] = native_dta;
			unsigned int victim = 0;
			for (unsigned int i = 0; i < 8; i++)
			{
				if (!cursors[i].address) { victim = i; break; }
				if (cursors[i].stamp < cursors[victim].stamp) victim = i;
			}
			cursors[victim].address = native_dta; cursors[victim].position = 0;
			cursors[victim].stamp = ++cursor_clock;
			memset(value, 0, sizeof(*value));
		}
		else
		{
			unsigned int i;
			for (i = 0; i < 8; i++) if (cursors[i].address == native_dta) break;
			if (i == 8) { *result = -49; return 1; }
			if (!filesystem_cursor_exists(native_dta) || cursors[i].position++)
			{
				forget_filesystem_cursor(native_dta); forget_cursor(native_dta);
				*result = -49; return 1;
			}
			cursors[i].stamp = ++cursor_clock;
		}
		value->attribute = 0x21; value->time = 0x1234; value->date = 0x5678;
		value->size = fn == GEMDOS_OP_FSFIRST ? 0x12345678 : 0x12345679;
		strcpy(value->name, fn == GEMDOS_OP_FSFIRST ? "ONE.TXT" : "TWO.TXT");
		*result = 0; return 1;
	}
	case GEMDOS_OP_DFREE: {
		uint32_t *values = (uint32_t *)a;
		values[0] = 0x11223344; values[1] = 0x55667788;
		values[2] = 0x80000000; values[3] = 0xffffffff;
		break;
	}
	case GEMDOS_OP_FXATTR: {
		ptos_native_xattr *value = (ptos_native_xattr *)c;
		memset(value, 0, sizeof(*value));
		value->mode = 0x8123; value->index = 0x12345678;
		value->dev = 0x1122; value->rdev = 0x3344; value->nlink = 0x5566;
		value->uid = 0x7788; value->gid = 0x99aa;
		value->size = 0x10203040; value->blksize = 0x50607080; value->blocks = 0x11223344;
		for (unsigned int i = 0; i < 3; i++) {
			value->stamp[i].time = 0x1234 + i; value->stamp[i].date = 0x5678 + i;
		}
		value->attribute = 0xabcd; value->reserved1 = 0x1357;
		value->reserved2[0] = 0x2468ace0; value->reserved2[1] = 0x1122aabb;
		break;
	}
	case GEMDOS_OP_PRUSAGE: {
		uint32_t *values = (uint32_t *)a;
		for (unsigned int i = 0; i < 8; i++) values[i] = 0x12345678 + i;
		break;
	}
	case GEMDOS_OP_PGETEUID:
	case GEMDOS_OP_PGETEGID:
	case GEMDOS_OP_PGETAUID:
	case GEMDOS_OP_PSETAUID:
	case GEMDOS_OP_PGETPRIORITY:
	case GEMDOS_OP_PSETPRIORITY:
	case GEMDOS_OP_PNICE:
	case GEMDOS_OP_PUMASK: break;
	case GEMDOS_OP_DGETCWD: if (c > 0) strcpy((char *)a, "NATIVE"); break;
	case GEMDOS_OP_DPATHCONF:
	case GEMDOS_OP_FCHMOD:
	case GEMDOS_OP_FCHOWN:
	case GEMDOS_OP_DLOCK:
	case GEMDOS_OP_DCHROOT:
	case GEMDOS_OP_FLINK:
	case GEMDOS_OP_FSYMLINK:
	case GEMDOS_OP_FREADLINK:
	case GEMDOS_OP_SSYNC: break;
	default: return 0;
	}
	last_function = fn;
	arguments[0] = a; arguments[1] = b; arguments[2] = c; arguments[3] = d;
	first_path[0] = second_path[0] = 0;
	switch (fn) {
	case GEMDOS_OP_FXATTR: strcpy(first_path, (const char *)b); break;
	case GEMDOS_OP_FREADLINK: strcpy(first_path, (const char *)c); break;
	case GEMDOS_OP_FLINK:
	case GEMDOS_OP_FSYMLINK: strcpy(second_path, (const char *)b); /* fall through */
	case GEMDOS_OP_DPATHCONF:
	case GEMDOS_OP_FCHMOD:
	case GEMDOS_OP_FCHOWN:
	case GEMDOS_OP_DCHROOT: strcpy(first_path, (const char *)a); break;
	default: break;
	}
	*result = error ? error : ((fn == GEMDOS_OP_DGETCWD && c <= 0) ||
		(fn == GEMDOS_OP_FREADLINK && a <= 0)) ? PTOS_ERANGE : 0;
	return 1;
}

static int32_t invoke(uint32_t sp, uint16_t function)
{
	m68k_set_reg(M68K_REG_SP, sp);
	m68k_write_memory_16(sp, function);
	last_function = 0;
	dispatch_gemdos_trap();
	assert(last_function == function);
	assert(fail_restore || native_dta == DEFAULT_DTA);
	return (int32_t)m68k_get_reg(NULL, M68K_REG_D0);
}

void test_filesystem(void)
{
	uint32_t previous = ptos_process;
	uint32_t bp = PTOS_ALLOC_START + 0x400, other = bp + 256;
	uint32_t sp = bp + 1024, out = sp + 513, dta2 = out + 128;
	uint32_t name = dta2 + 128, link = name + 128;
	ptos_process = bp;
	strcpy(ptos_guest_buffer(name, 32), "C:/SEARCH/*.TXT");
	strcpy(ptos_guest_buffer(link, 32), "C:/TARGET");
	m68k_write_memory_32(bp + BP_DTA, out);
	m68k_write_memory_32(other + BP_DTA, out); /* same address, different guest */
	const int32_t errors[] = {-36, PTOS_ENOSYS, -46};
	for (unsigned int i = 0; i < sizeof(errors) / sizeof(errors[0]); i++)
	{
		error = errors[i];
		memset(ptos_guest_buffer(out - 1, 66), 0xa7, 66);
		m68k_write_memory_32(sp + 2, out); m68k_write_memory_16(sp + 6, 3);
		assert(invoke(sp, GEMDOS_OP_DFREE) == error && arguments[1] == 3);
		for (unsigned int j = 0; j < 66; j++) assert(m68k_read_memory_8(out - 1 + j) == 0xa7);
		m68k_write_memory_32(sp + 2, out);
		assert(invoke(sp, GEMDOS_OP_PRUSAGE) == error);
		for (unsigned int j = 0; j < 66; j++) assert(m68k_read_memory_8(out - 1 + j) == 0xa7);
		m68k_write_memory_16(sp + 2, 1); m68k_write_memory_32(sp + 4, name);
		m68k_write_memory_32(sp + 8, out);
		assert(invoke(sp, GEMDOS_OP_FXATTR) == error);
		assert(!strcmp(first_path, "C:\\SEARCH\\*.TXT"));
		for (unsigned int j = 0; j < 66; j++) assert(m68k_read_memory_8(out - 1 + j) == 0xa7);
		m68k_write_memory_32(sp + 2, name); m68k_write_memory_16(sp + 6, 0);
		assert(invoke(sp, GEMDOS_OP_FSFIRST) == error);
		assert(invoke(sp, GEMDOS_OP_FSNEXT) == error);
		for (unsigned int j = 0; j < 66; j++) assert(m68k_read_memory_8(out - 1 + j) == 0xa7);
	}
	error = 0;
	m68k_write_memory_32(sp + 2, out); m68k_write_memory_16(sp + 6, 3);
	assert(invoke(sp, GEMDOS_OP_DFREE) == 0);
	assert(m68k_read_memory_32(out) == 0x11223344 && m68k_read_memory_32(out + 4) == 0x55667788);
	assert(m68k_read_memory_32(out + 8) == 0x80000000 && m68k_read_memory_32(out + 12) == 0xffffffff);
	m68k_write_memory_16(sp + 2, 0); m68k_write_memory_32(sp + 4, name); m68k_write_memory_32(sp + 8, out);
	assert(invoke(sp, GEMDOS_OP_FXATTR) == 0);
	assert(m68k_read_memory_16(out) == 0x8123 && m68k_read_memory_32(out + 2) == 0x12345678);
	assert(m68k_read_memory_16(out + 6) == 0x1122 && m68k_read_memory_16(out + 14) == 0x99aa);
	assert(m68k_read_memory_32(out + 16) == 0x10203040 && m68k_read_memory_32(out + 24) == 0x11223344);
	assert(m68k_read_memory_32(out + 28) == 0x12345678 && m68k_read_memory_32(out + 36) == 0x1236567a);
	assert(m68k_read_memory_16(out + 40) == 0xabcd && m68k_read_memory_16(out + 42) == 0x1357);
	assert(m68k_read_memory_32(out + 44) == 0x2468ace0 && m68k_read_memory_32(out + 48) == 0x1122aabb);
	assert(m68k_read_memory_8(out - 1) == 0xa7 && m68k_read_memory_8(out + 52) == 0xa7);
	m68k_write_memory_32(sp + 2, name); m68k_write_memory_16(sp + 6, 0);
	assert(invoke(sp, GEMDOS_OP_FSFIRST) == 0);
	assert(m68k_read_memory_32(out + 26) == 0x12345678 && !strcmp((char *)ptos_guest_buffer(out + 30, 14), "ONE.TXT"));
	assert(m68k_read_memory_32(out + 22) == 0x12345678);
	for (unsigned int j = 0; j < 21; j++) assert(m68k_read_memory_8(out + j) == 0);
	m68k_write_memory_32(bp + BP_DTA, dta2);
	assert(invoke(sp, GEMDOS_OP_FSFIRST) == 0);
	m68k_write_memory_32(bp + BP_DTA, out);
	ptos_process = other;
	assert(invoke(sp, GEMDOS_OP_FSNEXT) == -49);
	assert(invoke(sp, GEMDOS_OP_FSFIRST) == 0);
	ptos_filesystem_drop(other);
	assert((native_searches() == 2 && filesystem_searches() == 2));
	ptos_process = bp;
	assert(invoke(sp, GEMDOS_OP_FSNEXT) == 0 && m68k_read_memory_32(out + 26) == 0x12345679);
	assert(invoke(sp, GEMDOS_OP_FSNEXT) == -49);
	m68k_write_memory_32(bp + BP_DTA, dta2);
	assert(invoke(sp, GEMDOS_OP_FSNEXT) == 0); /* independent search cursor */
	ptos_filesystem_drop(bp);
	assert((native_searches() == 0 && filesystem_searches() == 0));
	assert(invoke(sp, GEMDOS_OP_FSNEXT) == -49);

	error = PTOS_ENOSYS;
	const uint16_t path_calls[] = {GEMDOS_OP_DPATHCONF, GEMDOS_OP_FCHMOD, GEMDOS_OP_FCHOWN, GEMDOS_OP_DCHROOT};
	for (unsigned int i = 0; i < sizeof(path_calls) / sizeof(path_calls[0]); i++)
	{
		m68k_write_memory_32(sp + 2, name); m68k_write_memory_16(sp + 6, 0xffff);
		m68k_write_memory_16(sp + 8, 0xffff);
		assert(invoke(sp, path_calls[i]) == PTOS_ENOSYS);
		assert(!strcmp(first_path, "C:\\SEARCH\\*.TXT"));
		if (path_calls[i] == GEMDOS_OP_DPATHCONF || path_calls[i] == GEMDOS_OP_FCHOWN) assert(arguments[1] == -1);
		if (path_calls[i] == GEMDOS_OP_FCHOWN) assert(arguments[2] == -1);
	}
	const uint16_t link_calls[] = {GEMDOS_OP_FLINK, GEMDOS_OP_FSYMLINK};
	for (unsigned int i = 0; i < sizeof(link_calls) / sizeof(link_calls[0]); i++)
	{
		m68k_write_memory_32(sp + 2, name); m68k_write_memory_32(sp + 6, link);
		assert(invoke(sp, link_calls[i]) == PTOS_ENOSYS);
		assert(!strcmp(first_path, "C:\\SEARCH\\*.TXT") && !strcmp(second_path, "C:\\TARGET"));
	}
	m68k_write_memory_16(sp + 2, 64); m68k_write_memory_32(sp + 4, out); m68k_write_memory_32(sp + 8, name);
	assert(invoke(sp, GEMDOS_OP_FREADLINK) == PTOS_ENOSYS && arguments[0] == 64);
	assert(!strcmp(first_path, "C:\\SEARCH\\*.TXT"));
	m68k_write_memory_32(sp + 2, out); m68k_write_memory_16(sp + 6, 0); m68k_write_memory_16(sp + 8, 64);
	assert(invoke(sp, GEMDOS_OP_DGETCWD) == PTOS_ENOSYS && arguments[1] == 0 && arguments[2] == 64);
	m68k_write_memory_16(sp + 2, 1); m68k_write_memory_16(sp + 4, 2);
	assert(invoke(sp, GEMDOS_OP_DLOCK) == PTOS_ENOSYS && arguments[0] == 1 && arguments[1] == 2);
	assert(invoke(sp, GEMDOS_OP_SSYNC) == PTOS_ENOSYS);
	const uint16_t process_calls[] = {GEMDOS_OP_PGETEUID, GEMDOS_OP_PGETEGID,
		GEMDOS_OP_PGETAUID, GEMDOS_OP_PSETAUID, GEMDOS_OP_PGETPRIORITY,
		GEMDOS_OP_PSETPRIORITY, GEMDOS_OP_PNICE, GEMDOS_OP_PUMASK};
	m68k_write_memory_16(sp + 2, 0xffff);
	m68k_write_memory_16(sp + 4, 0xffff); m68k_write_memory_16(sp + 6, 0xffff);
	for (unsigned int i = 0; i < sizeof(process_calls) / sizeof(process_calls[0]); i++)
		assert(invoke(sp, process_calls[i]) == PTOS_ENOSYS);
	assert(arguments[0] == -1);
	error = 0;
	m68k_write_memory_32(sp + 2, out);
	assert(invoke(sp, GEMDOS_OP_PRUSAGE) == 0);
	for (unsigned int i = 0; i < 8; i++) assert(m68k_read_memory_32(out + i * 4) == 0x12345678 + i);
	/* Non-positive signed lengths must reach the kernel even when the
	 * supplied guest pointer is unusable; only a native scratch is passed. */
	const int16_t sizes[] = {0, -1, INT16_MIN};
	const int32_t size_errors[] = {0, PTOS_ENOSYS};
	for (unsigned int e = 0; e < 2; e++)
		for (unsigned int i = 0; i < 3; i++)
		{
			error = size_errors[e];
			memset(ptos_guest_buffer(out, 64), 0xa7, 64);
			m68k_write_memory_32(sp + 2, PTOS_NATIVE_BASE);
			m68k_write_memory_16(sp + 6, 0); m68k_write_memory_16(sp + 8, sizes[i]);
			int32_t expected = error ? error : PTOS_ERANGE;
			assert(invoke(sp, GEMDOS_OP_DGETCWD) == expected);
			assert(arguments[0] != PTOS_NATIVE_BASE && arguments[2] == sizes[i]);
			m68k_write_memory_16(sp + 2, sizes[i]); m68k_write_memory_32(sp + 4, PTOS_NATIVE_BASE);
			m68k_write_memory_32(sp + 8, name);
			assert(invoke(sp, GEMDOS_OP_FREADLINK) == expected);
			assert(arguments[0] == sizes[i] && arguments[1] != PTOS_NATIVE_BASE);
			for (unsigned int j = 0; j < 64; j++) assert(m68k_read_memory_8(out + j) == 0xa7);
		}
	error = 0;
	m68k_write_memory_32(bp + BP_DTA, out);
	m68k_write_memory_32(other + BP_DTA, dta2);
	m68k_write_memory_32(sp + 2, name); m68k_write_memory_16(sp + 6, 0);
	assert(invoke(sp, GEMDOS_OP_FSFIRST) == 0);
	/* A protected native table has eight slots. Abandoned child searches
	 * must not consume them or evict the parent's cursor. */
	for (unsigned int i = 0; i < 32; i++)
	{
		ptos_process = other;
		assert(invoke(sp, GEMDOS_OP_FSFIRST) == 0 && (native_searches() == 2 && filesystem_searches() == 2));
		ptos_filesystem_drop(other);
		assert((native_searches() == 1 && filesystem_searches() == 1));
	}
	uint32_t child_dtas = sp + 4096;
	/* One overlay can also fill the table before exiting. Refuse its
	 * eighth cursor rather than evicting the parent's active cursor. */
	for (unsigned int i = 0; i < 7; i++)
	{
		m68k_write_memory_32(other + BP_DTA, child_dtas + i * 64);
		assert(ptos_search(GEMDOS_OP_FSFIRST, "C:\\SEARCH\\*.TXT", 0) == 0);
	}
	assert((native_searches() == 8 && filesystem_searches() == 8));
	uint32_t full_dta = child_dtas + 7 * 64;
	m68k_write_memory_32(other + BP_DTA, full_dta);
	memset(ptos_guest_buffer(full_dta, 44), 0xa7, 44);
	assert(ptos_search(GEMDOS_OP_FSFIRST, "C:\\SEARCH\\*.TXT", 0) == PTOS_ENSMEM);
	assert((native_searches() == 8 && filesystem_searches() == 8) && native_dta == DEFAULT_DTA);
	for (unsigned int i = 0; i < 44; i++) assert(m68k_read_memory_8(full_dta + i) == 0xa7);
	/* Restarting a search reuses its existing slot, even at capacity. */
	m68k_write_memory_32(other + BP_DTA, child_dtas);
	assert(ptos_search(GEMDOS_OP_FSFIRST, "C:\\SEARCH\\*.TXT", 0) == 0);
	ptos_filesystem_drop(other);
	assert((native_searches() == 1 && filesystem_searches() == 1));
	ptos_process = bp;
	assert(invoke(sp, GEMDOS_OP_FSNEXT) == 0);
	ptos_filesystem_cleanup();
	assert((native_searches() == 0 && filesystem_searches() == 0) && native_dta == DEFAULT_DTA);
	/* If restoration temporarily fails, retain the buffer, retire the
	 * owner and repair the native pointer before retrying cleanup. */
	const int32_t operation_errors[] = {0, PTOS_ENOSYS};
	for (unsigned int i = 0; i < 2; i++)
	{
		error = operation_errors[i]; fail_restore = 1;
		memset(ptos_guest_buffer(out, 44), 0xa7, 44);
		m68k_write_memory_32(sp + 2, name); m68k_write_memory_16(sp + 6, 0);
		assert(invoke(sp, GEMDOS_OP_FSFIRST) == (error ? error : -36));
		assert(native_dta != DEFAULT_DTA);
		for (unsigned int j = 0; j < 44; j++) assert(m68k_read_memory_8(out + j) == 0xa7);
		ptos_filesystem_drop(bp);
		fail_restore = 0; error = 0;
		ptos_filesystem_cleanup();
		assert((native_searches() == 0 && filesystem_searches() == 0) && native_dta == DEFAULT_DTA);
	}
	/* No Fsfirst record exists here: cleanup must still repair empty_dta. */
	fail_restore = 1;
	assert(invoke(sp, GEMDOS_OP_FSNEXT) == PTOS_ENMFIL);
	assert(native_dta != DEFAULT_DTA);
	fail_restore = 0;
	ptos_filesystem_cleanup();
	assert(native_dta == DEFAULT_DTA);
	/* Replacing an active search with a path error must first release its
	 * pluggable filesystem cursor, not just its protected DTA cache entry. */
	assert(ptos_search(GEMDOS_OP_FSFIRST, "C:\\SEARCH\\*.TXT", 0) == 0);
	assert(ptos_search(GEMDOS_OP_FSFIRST, "?:\\", 0) == -46);
	assert(native_searches() == 0 && filesystem_searches() == 0);
	ptos_filesystem_cleanup();
	/* A non-EOF cleanup error retires the buffer for a later retry. */
	assert(ptos_search(GEMDOS_OP_FSFIRST, "C:\\SEARCH\\*.TXT", 0) == 0);
	error = -36;
	ptos_filesystem_drop(bp);
	assert(native_searches() == 1 && filesystem_searches() == 0);
	error = 0;
	ptos_filesystem_cleanup();
	assert(native_searches() == 0 && filesystem_searches() == 0 && native_dta == DEFAULT_DTA);
	/* Fill all slots, then retire the owner while a temporary restoration
	 * failure prevents cleanup. A later guest must reclaim those contexts
	 * automatically before checking the native capacity limit. */
	ptos_process = other;
	for (unsigned int i = 0; i < 8; i++)
	{
		m68k_write_memory_32(other + BP_DTA, child_dtas + i * 64);
		assert(ptos_search(GEMDOS_OP_FSFIRST, "C:\\SEARCH\\*.TXT", 0) == 0);
	}
	assert(native_searches() == 8 && filesystem_searches() == 8);
	fail_restore = 1;
	assert(ptos_search(GEMDOS_OP_FSNEXT, NULL, 0) == -36);
	ptos_filesystem_drop(other);
	assert(native_searches() == 8 && filesystem_searches() == 8);
	fail_restore = 0;
	ptos_process = bp;
	assert(ptos_search(GEMDOS_OP_FSFIRST, "C:\\SEARCH\\*.TXT", 0) == 0);
	assert(native_searches() == 1 && filesystem_searches() == 1 && native_dta == DEFAULT_DTA);
	assert(ptos_search(GEMDOS_OP_FSNEXT, NULL, 0) == 0);
	ptos_filesystem_cleanup();
	assert(native_searches() == 0 && filesystem_searches() == 0);
	ptos_process = previous;
}

void test_filesystem_retained(uint32_t child)
{
	uint32_t parent = ptos_process;
	uint32_t sp = child + 1024, out = child + 512, name = child + 640;
	assert(ptos_go(child, 0) == 0);
	m68k_write_memory_32(child + BP_DTA, out);
	strcpy(ptos_guest_buffer(name, 32), "C:/SEARCH/*.TXT");
	m68k_write_memory_32(sp + 2, name); m68k_write_memory_16(sp + 6, 0);
	assert(invoke(sp, GEMDOS_OP_FSFIRST) == 0);
	ptos_terminate(23);
	assert(ptos_process == parent && ptos_image_valid(child));
	assert((native_searches() == 0 && filesystem_searches() == 0));
	uint32_t saved_sp = m68k_get_reg(NULL, M68K_REG_SP);
	/* The retained basepage still exists. Its terminated guest must no
	 * longer have a search, before the caller frees that image. */
	ptos_process = child;
	assert(invoke(sp, GEMDOS_OP_FSNEXT) == -49);
	ptos_process = parent;
	m68k_set_reg(M68K_REG_SP, saved_sp);
}
