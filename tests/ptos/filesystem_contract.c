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
static uint16_t last_function;
static intptr_t arguments[4];
static char first_path[1024], second_path[1024];

int filesystem_test_native(uint16_t fn, intptr_t a, intptr_t b, intptr_t c, intptr_t d, int32_t *result)
{
	if (fn == GEMDOS_OP_FGETDTA) { *result = (int32_t)native_dta; return 1; }
	if (fn == GEMDOS_OP_FSETDTA) { native_dta = a; *result = 0; return 1; }
	switch (fn)
	{
	case GEMDOS_OP_FSFIRST:
	case GEMDOS_OP_FSNEXT: {
		last_function = fn;
		assert(native_dta != DEFAULT_DTA);
		ptos_native_dta *value = (ptos_native_dta *)native_dta;
		if (error) { memset(value, 0xcc, sizeof(*value)); *result = error; return 1; }
		if (fn == GEMDOS_OP_FSFIRST)
		{
			assert(!strcmp((const char *)a, "C:\\SEARCH\\*.TXT"));
			memset(value, 0, sizeof(*value));
			value->reserved[0] = 0x5a;
		}
		else if (value->reserved[0] != 0x5a || value->reserved[1]++)
		{ *result = -49; return 1; }
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
	case GEMDOS_OP_DGETCWD: strcpy((char *)a, "NATIVE"); break;
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
	*result = error;
	return 1;
}

static int32_t invoke(uint32_t sp, uint16_t function)
{
	m68k_set_reg(M68K_REG_SP, sp);
	m68k_write_memory_16(sp, function);
	last_function = 0;
	dispatch_gemdos_trap();
	assert(last_function == function);
	assert(native_dta == DEFAULT_DTA);
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
	ptos_process = bp;
	assert(invoke(sp, GEMDOS_OP_FSNEXT) == 0 && m68k_read_memory_32(out + 26) == 0x12345679);
	assert(invoke(sp, GEMDOS_OP_FSNEXT) == -49);
	m68k_write_memory_32(bp + BP_DTA, dta2);
	assert(invoke(sp, GEMDOS_OP_FSNEXT) == 0); /* independent search cursor */
	ptos_filesystem_drop(bp);
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
	error = 0;
	ptos_filesystem_cleanup();
	ptos_process = previous;
}
