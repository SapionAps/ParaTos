#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "m68k.h"
#include "host.h"
#include "memory.h"
#include "guest.h"
#include "vdi.h"
#include "vdi_contract.h"

void dispatch_gem_trap(void);
static unsigned int calls, active, close_calls;
static int16_t next_handle = 20, opened[128];
static int32_t native_error;
static int malformed_counts, fail_close, check_raster;
static int16_t last_input, last_x, last_y;

int32_t ptos_native_vdi(paratos_vdi_pb *block)
{
	paratos_vdi_control *c = block->control;
	calls++;
	assert(c->reserved == 0);
	assert(c->subcode == (c->code == VDI_GDP ? 1 : 0));
	if (c->code == VDI_OPEN)
	{
		assert(c->nintin == 11 && c->nptsin == 0 && block->intin[10] == 2);
		assert(c->args.reserved[0] == 0 && c->args.reserved[1] == 0 && c->args.reserved[2] == 0);
		if (native_error) { block->intout[0] = 0x5555; return native_error; }
		c->handle = ++next_handle;
		for (unsigned int i = 0; i < 128; i++)
			if (!opened[i]) { opened[i] = c->handle; active++; break; }
		for (int i = 0; i < 45; i++) block->intout[i] = 0x1200 + i;
		for (int i = 0; i < 12; i++) block->ptsout[i] = -100 + i;
		c->nintout = malformed_counts ? 46 : 45; c->nptsout = 6;
		return 0;
	}
	unsigned int slot;
	for (slot = 0; slot < 128; slot++) if (opened[slot] == c->handle) break;
	assert(slot < 128);
	if (c->code == VDI_CLOSE)
	{
		close_calls++;
		if (fail_close == 1) return -36;
		opened[slot] = 0; active--; c->handle = 0;
		if (fail_close == 2) return PTOS_EIHNDL;
		c->nintout = c->nptsout = 0;
		return 0;
	}
	if (native_error)
	{
		if (native_error == PTOS_EIHNDL) { opened[slot] = 0; active--; }
		block->intout[0] = 0x5555;
		if (c->code == VDI_COPY) ((uint16_t *)c->args.raster.destination->address)[0] = 0x5555;
		return native_error;
	}
	c->nintout = c->nptsout = 0;
	switch (c->code)
	{
	case VDI_LINE_COLOR:
		last_input = block->intin[0]; block->intout[0] = 0x2345; c->nintout = 1; break;
	case VDI_POLYLINE:
		last_x = block->ptsin[0]; last_y = block->ptsin[1];
		assert(block->ptsin[c->nptsin * 2 - 1] == 0x5678); break;
	case VDI_TEXT:
		assert(c->nintin == 3 && block->intin[0] == 'A' && block->intin[1] == 0x1234);
		assert(block->intin[2] == 0x00ff && block->ptsin[0] == -123); break;
	case VDI_GDP: assert(c->nintin == 0 && c->nptsin == 2); break;
	case VDI_COPY: case VDI_TRANSFORM: {
		paratos_vdi_fdb *s = c->args.raster.source, *d = c->args.raster.destination;
		assert(s && d && s->address && d->address && s->address != d->address);
		assert(s->width == 16 && s->height == 1 && s->stride == 1);
		assert(d->width == 16 && d->height == 1 && d->stride == 1);
		if (check_raster == 4)
		{
			assert(s->planes == 32 && ((uint32_t *)s->address)[0] == 0x11223344);
			((uint32_t *)d->address)[0] = 0xa1b2c3d4;
		}
		else
		{
			assert(s->planes == 1 && ((uint16_t *)s->address)[0] == 0xa55a);
			assert(((uint16_t *)d->address)[0] == 0x1234);
			((uint16_t *)d->address)[0] = 0x80f1;
		}
		if (c->code == VDI_TRANSFORM) d->standard = !s->standard;
		break;
	}
	default: assert(0);
	}
	if (malformed_counts) c->nintout = 46;
	return 0;
}

/* Two-byte aligned PB/control exercise the classic ABI independently of
 * the four-byte aligned native temporary structures. */
enum { PB = 0x90002, CONTROL = 0x90102, INPUT = 0x90200, POINTS = 0x90500,
	OUTPUT = 0x90a00, OUTPOINTS = 0x90b00, SOURCE = 0x90c02, DEST = 0x90c22,
	SOURCE_DATA = 0x91000, DEST_DATA = 0x92000 };

static void request(uint16_t operation, uint16_t ni, uint16_t np, int16_t handle)
{
	const uint32_t pointers[] = {CONTROL, INPUT, POINTS, OUTPUT, OUTPOINTS};
	for (unsigned int i = 0; i < 5; i++) m68k_write_memory_32(PB + i * 4, pointers[i]);
	for (unsigned int i = 0; i < 12; i++) m68k_write_memory_16(CONTROL + i * 2, 0x7a7a);
	m68k_write_memory_16(CONTROL, operation);
	m68k_write_memory_16(CONTROL + 2, np); m68k_write_memory_16(CONTROL + 6, ni);
	m68k_write_memory_16(CONTROL + 10, 0x4567); m68k_write_memory_16(CONTROL + 12, handle);
	memset(ptos_guest_buffer(OUTPUT - 2, 96), 0xa7, 96);
	memset(ptos_guest_buffer(OUTPOINTS - 2, 28), 0xb8, 28);
}

static int32_t invoke(void)
{
	m68k_set_reg(M68K_REG_D0, 0xffff0073); /* classic move.w leaves D0's upper half */
	m68k_set_reg(M68K_REG_D1, PB);
	m68k_set_reg(M68K_REG_A2, 0x12345678);
	dispatch_gem_trap();
	assert(m68k_get_reg(NULL, M68K_REG_D1) == PB && m68k_get_reg(NULL, M68K_REG_A2) == 0x12345678);
	return (int32_t)m68k_get_reg(NULL, M68K_REG_D0);
}

static int16_t open_workstation(void)
{
	request(100, 11, 0, 0);
	for (unsigned int i = 0; i < 11; i++) m68k_write_memory_16(INPUT + i * 2, i);
	m68k_write_memory_16(INPUT + 20, 2);
	assert(invoke() == 0);
	return m68k_read_memory_16(CONTROL + 12);
}

static void mfdb(uint32_t descriptor, uint32_t data, int planes, int standard)
{
	m68k_write_memory_32(descriptor, data);
	const uint16_t fields[] = {16, 1, 1, standard, planes, 0, 0, 0};
	for (unsigned int i = 0; i < 8; i++) m68k_write_memory_16(descriptor + 4 + i * 2, fields[i]);
}

void test_vdi(void)
{
	uint32_t previous = ptos_process;
	ptos_process = 0x88000;
	request(100, 11, 0, 0); m68k_write_memory_16(INPUT + 20, 2);
	native_error = PTOS_ENOSYS;
	assert(invoke() == PTOS_ENOSYS && active == 0);
	assert(m68k_read_memory_16(OUTPUT) == 0xa7a7 && m68k_read_memory_16(CONTROL + 8) == 0x7a7a);
	native_error = 0;
	int16_t handle = open_workstation();
	assert(active == 1 && m68k_read_memory_16(CONTROL + 4) == 6 && m68k_read_memory_16(CONTROL + 8) == 45);
	assert(m68k_read_memory_16(OUTPUT) == 0x1200 && m68k_read_memory_16(OUTPUT + 88) == 0x122c);
	assert(m68k_read_memory_16(OUTPOINTS) == (uint16_t)-100);
	assert(m68k_read_memory_16(OUTPUT - 2) == 0xa7a7 && m68k_read_memory_16(OUTPUT + 90) == 0xa7a7);
	assert(m68k_read_memory_16(OUTPOINTS + 24) == 0xb8b8);
	assert(m68k_read_memory_16(CONTROL + 14) == 0x7a7a); /* native pad must not shift guest pointers */
	request(17, 1, 0, handle); m68k_write_memory_16(INPUT, 0xfedc);
	assert(invoke() == 0 && last_input == (int16_t)0xfedc && m68k_read_memory_16(OUTPUT) == 0x2345);
	assert(m68k_read_memory_16(CONTROL + 10) == 0x4567);
	request(17, 1, 0, handle); m68k_write_memory_32(PB + 12, CONTROL + 22);
	assert(invoke() == 0 && m68k_read_memory_16(CONTROL + 22) == 0x2345);
	request(11, 0, 2, handle); m68k_write_memory_16(CONTROL + 10, 1);
	assert(invoke() == 0 && m68k_read_memory_16(CONTROL + 10) == 1);
	request(6, 0, 256, handle);
	for (unsigned int i = 0; i < 512; i++) m68k_write_memory_16(POINTS + i * 2, 0x5678);
	m68k_write_memory_16(POINTS, -1000); m68k_write_memory_16(POINTS + 2, -2000);
	assert(invoke() == 0 && last_x == -1000 && last_y == -2000);
	request(8, 3, 1, handle);
	m68k_write_memory_16(INPUT, 'A'); m68k_write_memory_16(INPUT + 2, 0x1234);
	m68k_write_memory_16(INPUT + 4, 0xff); m68k_write_memory_16(POINTS, -123);
	assert(invoke() == 0);
	const int32_t errors[] = {-36, PTOS_ENOSYS, (int32_t)0x89abcdef, PTOS_EIHNDL};
	for (unsigned int i = 0; i < sizeof(errors) / sizeof(errors[0]); i++)
	{
		request(17, 1, 0, handle); native_error = errors[i];
		assert(invoke() == native_error);
		assert(m68k_read_memory_16(OUTPUT) == 0xa7a7 && m68k_read_memory_16(CONTROL + 8) == 0x7a7a);
	}
	native_error = 0;
	handle = open_workstation(); /* Native EIHNDL invalidated the old handle. */
	request(17, 1, 0, handle); malformed_counts = 1;
	assert(invoke() == PTOS_EINVAL && m68k_read_memory_16(OUTPUT) == 0xa7a7);
	malformed_counts = 0;
	unsigned int before = calls;
	request(6, 0, 257, handle); assert(invoke() == PTOS_EINVAL);
	request(8, 257, 1, handle); assert(invoke() == PTOS_EINVAL);
	request(17, 0xffff, 0, handle); assert(invoke() == PTOS_EINVAL);
	request(11, 0, 2, handle); m68k_write_memory_16(CONTROL + 10, 2);
	assert(invoke() == PTOS_ENOSYS);
	request(6, 0, 2, handle); m68k_write_memory_16(CONTROL + 10, 13);
	assert(invoke() == PTOS_ENOSYS);
	request(118, 0, 0, handle); assert(invoke() == PTOS_ENOSYS); /* guest callback never reaches native */
	request(119, 0, 0, handle); assert(invoke() == PTOS_ENOSYS);
	request(17, 1, 0, handle); m68k_write_memory_32(PB + 12, PTOS_NATIVE_BASE);
	assert(invoke() == PTOS_EIMBA);
	request(17, 1, 0, handle); m68k_write_memory_32(PB + 4, UINT32_MAX - 1);
	assert(invoke() == PTOS_EIMBA);
	request(17, 1, 0, handle); m68k_write_memory_32(PB, CONTROL + 1);
	assert(invoke() == PTOS_EIMBA && calls == before);
	for (unsigned int unit = 2; unit <= 4; unit += 2)
	{
		check_raster = unit;
		request(109, 1, 4, handle);
		mfdb(SOURCE, SOURCE_DATA, unit == 4 ? 32 : 1, 0);
		mfdb(DEST, DEST_DATA, unit == 4 ? 32 : 1, 0);
		m68k_write_memory_32(CONTROL + 14, SOURCE); m68k_write_memory_32(CONTROL + 18, DEST);
		m68k_write_memory_32(SOURCE_DATA, unit == 4 ? 0x11223344 : 0xa55a0000);
		m68k_write_memory_32(DEST_DATA, 0x12340000);
		assert(invoke() == 0);
		if (unit == 4) assert(m68k_read_memory_32(DEST_DATA) == 0xa1b2c3d4);
		else assert(m68k_read_memory_16(DEST_DATA) == 0x80f1);
		assert(m68k_read_memory_32(SOURCE) == SOURCE_DATA && m68k_read_memory_32(DEST) == DEST_DATA);
		assert(m68k_read_memory_32(CONTROL + 14) == SOURCE && m68k_read_memory_32(CONTROL + 18) == DEST);
	}
	/* Partially overlapping packed bitmaps still use separate native snapshots. */
	check_raster = 4;
	request(109, 1, 4, handle); mfdb(SOURCE, SOURCE_DATA, 32, 0); mfdb(DEST, SOURCE_DATA + 2, 32, 0);
	m68k_write_memory_32(CONTROL + 14, SOURCE); m68k_write_memory_32(CONTROL + 18, DEST);
	m68k_write_memory_32(SOURCE_DATA, 0x11223344);
	assert(invoke() == 0 && m68k_read_memory_16(SOURCE_DATA) == 0x1122);
	assert(m68k_read_memory_32(SOURCE_DATA + 2) == 0xa1b2c3d4);
	/* A destination can overlap an unused control word without being erased
	 * by control writeback after its successful raster copy. */
	check_raster = 2;
	request(109, 1, 4, handle); mfdb(SOURCE, SOURCE_DATA, 1, 0); mfdb(DEST, CONTROL + 22, 1, 0);
	m68k_write_memory_32(CONTROL + 14, SOURCE); m68k_write_memory_32(CONTROL + 18, DEST);
	m68k_write_memory_16(SOURCE_DATA, 0xa55a); m68k_write_memory_16(CONTROL + 22, 0x1234);
	assert(invoke() == 0 && m68k_read_memory_16(CONTROL + 22) == 0x80f1);
	check_raster = 2;
	request(109, 1, 4, handle); mfdb(SOURCE, SOURCE_DATA, 1, 0); mfdb(DEST, DEST_DATA, 1, 0);
	m68k_write_memory_32(CONTROL + 14, SOURCE); m68k_write_memory_32(CONTROL + 18, DEST);
	m68k_write_memory_16(SOURCE_DATA, 0xa55a); m68k_write_memory_16(DEST_DATA, 0x1234);
	native_error = -36;
	assert(invoke() == -36 && m68k_read_memory_16(DEST_DATA) == 0x1234);
	native_error = 0;
	/* Transform output updates only fd_stand in the guest descriptor. */
	request(110, 0, 0, handle); mfdb(SOURCE, SOURCE_DATA, 1, 1); mfdb(DEST, DEST_DATA, 1, 1);
	m68k_write_memory_32(CONTROL + 14, SOURCE); m68k_write_memory_32(CONTROL + 18, DEST);
	m68k_write_memory_16(DEST_DATA, 0x1234);
	assert(invoke() == 0 && m68k_read_memory_16(DEST + 10) == 0);
	before = calls;
	mfdb(DEST, PTOS_RAM_END - 2, 32, 0); request(109, 1, 4, handle);
	m68k_write_memory_32(CONTROL + 14, SOURCE); m68k_write_memory_32(CONTROL + 18, DEST);
	assert(invoke() == PTOS_EIMBA && calls == before);
	mfdb(SOURCE, SOURCE_DATA, 32, 0);
	m68k_write_memory_16(SOURCE + 4, 32767); m68k_write_memory_16(SOURCE + 6, 32767);
	m68k_write_memory_16(SOURCE + 8, 2048);
	assert(invoke() == PTOS_EINVAL && calls == before);
	m68k_set_reg(M68K_REG_D0, 0xc8); m68k_set_reg(M68K_REG_D1, PTOS_NATIVE_BASE);
	dispatch_gem_trap();
	assert((int32_t)m68k_get_reg(NULL, M68K_REG_D0) == PTOS_ENOSYS && calls == before);
	/* An internal child cannot use its parent's native workstation. */
	ptos_process++;
	request(17, 1, 0, handle); assert(invoke() == PTOS_EIHNDL && calls == before);
	for (unsigned int i = 0; i < 64; i++)
	{
		open_workstation(); assert(active == 2);
		ptos_vdi_drop(ptos_process); assert(active == 1);
	}
	int16_t abandoned = open_workstation(); (void)abandoned;
	fail_close = 1; ptos_vdi_drop(ptos_process); assert(active == 2);
	fail_close = 0; ptos_process--;
	request(17, 1, 0, handle); assert(invoke() == 0 && active == 1);
	ptos_process++; open_workstation(); fail_close = 2;
	ptos_vdi_drop(ptos_process); assert(active == 1);
	fail_close = 0; ptos_process--;
	request(17, 1, 0, handle); assert(invoke() == 0 && active == 1);
	request(101, 0, 0, handle); assert(invoke() == 0 && active == 0);
	request(101, 0, 0, handle); assert(invoke() == PTOS_EIHNDL);
	request(100, 11, 0, 0); m68k_write_memory_16(INPUT + 20, 2); malformed_counts = 1;
	assert(invoke() == PTOS_EINVAL && active == 0 && m68k_read_memory_16(OUTPUT) == 0xa7a7);
	malformed_counts = 0;
	ptos_vdi_cleanup();
	ptos_process = previous;
}

void test_vdi_retained(uint32_t child)
{
	uint32_t parent = ptos_process;
	unsigned int before = active, closed = close_calls;
	assert(ptos_go(child, 0) == 0);
	open_workstation(); assert(active == before + 1);
	ptos_terminate(23);
	assert(ptos_process == parent && ptos_image_valid(child));
	assert(active == before && close_calls == closed + 1);
}
