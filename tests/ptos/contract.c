#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include "m68k.h"
#include "host.h"
#include "memory.h"
#include "guest.h"

static char output[8192];
static size_t output_size;
static uint32_t ticks = 0x11223344;
static unsigned int system_calls, exec_calls;
static int32_t last_mode;
static uint32_t retained_image;
static int expect_retained;
static int current_drive = 2;
static char directories[2][128] = {"PARENT", "OTHER"};

static void capture(const void *data, size_t n)
{
	assert(n < sizeof(output) - output_size);
	memcpy(output + output_size, data, n);
	output_size += n; output[output_size] = 0;
}

int32_t ptos_map(uint32_t address, uint32_t length)
{
	void *p = mmap((void *)(uintptr_t)address, length, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
	return p == MAP_FAILED ? -39 : (int32_t)(uintptr_t)p;
}

int32_t ptos_unmap(uint32_t address, uint32_t length)
{
	return munmap((void *)(uintptr_t)address, length);
}

int32_t ptos_call(uint16_t fn, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
	(void)d;
	switch (fn)
	{
	case 0x154:
		system_calls++;
		if (a >= 0xd && a <= 0xf) return -36;
		if (b == 0x4ba) return ticks;
		if (b == 0x462) return 0x12345678;
		return 0;
	case 9: capture((const char *)a, strlen((const char *)a)); return 0;
	case 2: {
		char ch = a;
		capture(&ch, 1);
		if (retained_image) assert(ptos_image_valid(retained_image) == expect_retained);
		return a;
	}
	case 0x3c: return open((const char *)a, O_CREAT | O_TRUNC | O_RDWR, 0600);
	case 0x3a: return -34;
	case 0x3d: return open((const char *)a, b == 0 ? O_RDONLY : b == 1 ? O_WRONLY : O_RDWR);
	case 0x3e: return a > 2 ? close(a) : 0;
	case 0x3f: return read(a, (void *)c, b);
	case 0x40:
		if (a == 1 || a == 2) { capture((const void *)c, b); return b; }
		return write(a, (const void *)c, b);
	case 0x41: return unlink((const char *)a);
	case 0x42: { off_t pos = lseek(b, a, c); return pos < 0 ? -64 : (int32_t)pos; }
	case 0x57: return 0;
	case 0x4b:
		exec_calls++; last_mode = a;
		assert(!strcmp((const char *)b, "C:\\APP.PRG"));
		assert(((const char *)c)[0] == 0);
		assert(!strcmp((const char *)d, "X=1"));
		return a == 200 ? PTOS_ENOSYS : 37;
	case 0x30: return 0x2000;
	case GEMDOS_OP_DGETDRV: return current_drive;
	case GEMDOS_OP_DSETDRV: current_drive = a; return 12;
	case GEMDOS_OP_DGETPATH: {
		int drive = b ? b - 1 : current_drive;
		if (drive < 2 || drive > 3) return -46;
		strcpy((char *)a, directories[drive - 2]); return 0;
	}
	case GEMDOS_OP_DSETPATH:
		assert(current_drive == 2 || current_drive == 3);
		strcpy(directories[current_drive - 2], (const char *)a + (*(char *)a == '\\'));
		return 0;
	case 0x2a: return (46 << 9) | (11 << 5) | 10;
	case 0x2c: return 0;
	default: return PTOS_ENOSYS;
	}
}

static void fixture(const unsigned char *code, size_t n)
{
	unsigned char header[28] = {0x60, 0x1a};
	header[2] = n >> 24; header[3] = n >> 16; header[4] = n >> 8; header[5] = n;
	header[27] = 1;
	FILE *f = fopen("contract.prg", "wb"); assert(f);
	assert(fwrite(header, 1, sizeof(header), f) == sizeof(header));
	assert(fwrite(code, 1, n, f) == n); assert(!fclose(f));
}

static uint32_t load_fixture(const unsigned char *code, size_t n)
{
	fixture(code, n);
	char tail[128] = {0};
	int32_t bp = ptos_load("contract.prg", tail, "\0");
	assert(bp > 0);
	return bp;
}

static void reset(void)
{
	retained_image = 0;
	output_size = 0; output[0] = 0;
	ptos_allocator_init();
	ptos_memory_reset_low();
}

static void test_memory(void)
{
	assert(ptos_guest_buffer(0x12345, 4) == (void *)(uintptr_t)0x12345);
	m68k_write_memory_32(0x12345, 0x12345678);
	assert(m68k_read_memory_16(0x12346) == 0x3456);
	assert(*(uint8_t *)(uintptr_t)0x12345 == 0x12);
	assert(m68k_read_memory_32(0x4ba) == 0x11223344);
	assert(m68k_read_memory_8(0x4bb) == 0x22);
	assert(m68k_read_memory_16(0x4bd) == 0x4400);
	assert(m68k_read_memory_32(0x460) == 0x00001234);
	ticks = 0x55667788;
	assert(m68k_read_memory_16(0x4ba) == 0x5566);
	assert(ptos_low_write(0x4ba, 4, 1) == -36);
	assert(ptos_low_write(0x4bb, 1, 1) == PTOS_EINVAL);
	unsigned int before = system_calls;
	assert(m68k_read_memory_32(0x436) == PTOS_RAM_END);
	assert(m68k_read_memory_32(0x5a0) == 0x1000);
	assert(system_calls == before);
	assert(m68k_read_memory_32(0x1004) == 20);
	uint32_t v;
	assert(ptos_low_read(UINT32_MAX, 4, &v) == PTOS_EINVAL);
}

static void test_arguments(void)
{
	char tail[128];
	char *simple[] = {"one", "two"};
	ptos_command_tail(tail, 2, simple);
	assert(tail[0] == 7 && !strcmp(tail + 1, "one two"));
	char *quoted[] = {"two words", "it's"};
	ptos_command_tail(tail, 2, quoted);
	assert(tail[0] == 19 && !strcmp(tail + 1, "'two words' 'it''s'"));
	char *empty[] = {"two words", ""};
	ptos_command_tail(tail, 2, empty);
	assert(tail[0] == 127 && !strcmp(tail + 1, "'two words' ''"));
	char long_arg[200]; memset(long_arg, 'a', sizeof(long_arg) - 1); long_arg[199] = 0;
	char *long_args[] = {long_arg};
	ptos_command_tail(tail, 1, long_args);
	assert(tail[0] == 127 && strlen(tail + 1) == 126 && tail[127] == 0);
	ptos_command_tail(tail, 0, NULL);
	assert(tail[0] == 0 && tail[1] == 0);
}

static void test_delegation(void)
{
	reset();
	uint32_t bp = ptos_alloc(512); ptos_process = bp;
	m68k_write_memory_32(bp + BP_ENV, bp + 256);
	strcpy(ptos_guest_buffer(bp + 256, 8), "X=1");
	m68k_write_memory_8(bp + 260, 0);
	strcpy(ptos_guest_buffer(bp + 300, 16), "C:/APP.PRG");
	assert(ptos_exec(0, bp + 300, bp + 384, 0) == 37);
	assert(last_mode == 0);
	assert(ptos_exec(200, bp + 300, bp + 384, 0) == PTOS_ENOSYS);
	assert(last_mode == 200 && exec_calls == 2);
}

static void test_guest_super(void)
{
	reset();
	unsigned char program[] = {
		0x48,0x79, 0,0,0,0,             /* pea callback */
		0x3f,0x3c, 0,38, 0x4e,0x4e,    /* Supexec */
		0x4f,0xef, 0,6,
		0x3f,0x00, 0x3f,0x3c, 0,0x4c, 0x4e,0x41,
		0x40,0xf9, 0,0,0,0,             /* callback: move sr,absolute */
		0x70,37, 0x4e,0x75
	};
	uint32_t bp = load_fixture(program, sizeof(program));
	uint32_t base = m68k_read_memory_32(bp + BP_TEXT);
	m68k_write_memory_32(base + 2, base + 24);
	m68k_write_memory_32(base + 26, base + 512);
	assert(ptos_run_program(bp) == 37);
	assert(m68k_read_memory_16(base + 512) & 0x2000);
	assert(!(m68k_get_reg(NULL, M68K_REG_SR) & 0x2000));
	assert(ptos_super(1) == 0);
	int32_t old = ptos_super(0);
	assert(ptos_super(1) == -1);
	ptos_super(old);
	assert(ptos_super(1) == 0);
}

static void test_overlay(int mode)
{
	reset();
	unsigned char caller[] = {
		0x42,0xa7, 0x48,0x79, 0,0,0,0, 0x42,0xa7,
		0x3f,0x3c, 0,0, 0x3f,0x3c, 0,0x4b,
		0x20,0x7c, 0x12,0x34,0x56,0x78, /* stale parent A0 before Pexec */
		0x4e,0x41,
		0x4f,0xef, 0,16,
		0x3f,0x00, 0x3f,0x3c, 0,2, 0x4e,0x41,
		0x4f,0xef, 0,4,
		0x3f,0x3c, 0,0, 0x3f,0x3c, 0,0x4c, 0x4e,0x41
	};
	/* A0 distinguishes a normal program from an accessory in TOS startup.
	 * A child inheriting the parent's A0 exits with 38 rather than 37. */
	unsigned char callee[] = {
		0x20,0x08, 0x66,0x0a, /* move.l a0,d0; bne failure */
		0x3f,0x3c,0,37,0x3f,0x3c,0,0x4c,0x4e,0x41,
		0x3f,0x3c,0,38,0x3f,0x3c,0,0x4c,0x4e,0x41
	};
	uint32_t bp = load_fixture(caller, sizeof(caller));
	assert(ptos_shrink(bp, 4096) == 0);
	assert(m68k_read_memory_32(bp + BP_HIGH) == bp + 4096);
	ptos_process = bp;
	fixture(callee, sizeof(callee));
	strcpy(ptos_guest_buffer(bp + 768, 32), "contract.prg");
	memset(ptos_guest_buffer(bp + 1024, 128), 0, 128);
	int32_t loaded = ptos_exec(3, bp + 768, bp + 1024, 0);
	assert(loaded > 0);
	uint32_t child = loaded;
	uint32_t base = m68k_read_memory_32(bp + BP_TEXT);
	m68k_write_memory_32(base + 4, child);
	m68k_write_memory_16(base + 12, mode);
	retained_image = child; expect_retained = mode == 4;
	assert(ptos_run_program(bp) == 0);
	assert(output_size == 1 && output[0] == 37);
	assert(!ptos_image_valid(bp) && !ptos_image_valid(child));
	retained_image = 0;
}

static void test_bad_images(void)
{
	reset();
	unsigned char code[] = {0x4e,0x75};
	fixture(code, sizeof(code));
	FILE *f = fopen("contract.prg", "r+b"); assert(f);
	/* Claim an overflowing/unavailable TEXT size. */
	assert(fseek(f, 2, SEEK_SET) == 0);
	unsigned char bad[4] = {0xff,0xff,0xff,0xff};
	assert(fwrite(bad, 1, 4, f) == 4); fclose(f);
	char tail[128] = {0};
	assert(ptos_load("contract.prg", tail, "\0") == PTOS_EPLFMT);
	assert(ptos_alloc(-1) == PTOS_RAM_END - PTOS_ALLOC_START);
	fixture(code, sizeof(code));
	f = fopen("contract.prg", "r+b"); assert(f);
	/* Require a relocation table, but leave it truncated. */
	assert(fseek(f, 27, SEEK_SET) == 0); assert(fputc(0, f) != EOF); fclose(f);
	assert(ptos_load("contract.prg", tail, "\0") == PTOS_EPLFMT);
	assert(ptos_alloc(-1) == PTOS_RAM_END - PTOS_ALLOC_START);
}

static void test_nested_supervisor_stack(void)
{
	reset();
	unsigned char code[] = {0x4e,0x75};
	uint32_t bp = load_fixture(code, sizeof(code));
	assert(ptos_shrink(bp, 4096) == 0);
	ptos_process = bp;
	uint32_t child = load_fixture(code, sizeof(code));
	m68k_set_reg(M68K_REG_SR, 0x2300);
	m68k_set_reg(M68K_REG_SP, 0xff00);
	m68k_write_memory_32(0xff00, 0x12345678);
	assert(ptos_go(child, 0) == 0);
	current_drive = 3;
	strcpy(directories[0], "CHANGED"); strcpy(directories[1], "CHILD");
	assert(m68k_get_reg(NULL, M68K_REG_ISP) == 0xff00);
	ptos_terminate(37);
	assert(current_drive == 2 && !strcmp(directories[0], "PARENT") && !strcmp(directories[1], "OTHER"));
	assert(ptos_process == bp && m68k_get_reg(NULL, M68K_REG_SP) == 0xff00);
	assert(m68k_get_reg(NULL, M68K_REG_SR) & 0x2000);
	assert(m68k_read_memory_32(0xff00) == 0x12345678);
	m68k_set_reg(M68K_REG_SR, 0x0300);
}

static void test_fault(void)
{
	reset();
	unsigned char code[] = {0x23,0xfc,0,0,0,1, 0x41,0,0,0};
	uint32_t bp = load_fixture(code, sizeof(code));
	assert(ptos_run_program(bp) == 255);
}

static void test_cli(void)
{
	reset();
	unsigned char code[] = {
		0x48,0x79, 0,0,0,0, 0x3f,0x3c,0,9,0x4e,0x41,
		0x4f,0xef,0,6, 0x3f,0x3c,0,37,0x3f,0x3c,0,0x4c,0x4e,0x41,
		'H','e','l','l','o',' ','T','O','S',0
	};
	uint32_t bp = load_fixture(code, sizeof(code));
	uint32_t base = m68k_read_memory_32(bp + BP_TEXT);
	m68k_write_memory_32(base + 2, base + 26);
	assert(ptos_run_program(bp) == 37);
	assert(!strcmp(output, "Hello TOS"));
}

int main(int argc, char **argv)
{
	assert(ptos_memory_init() == 0);
	if (argc == 2)
	{
		char tail[128];
		char *args[] = {"two words", ""};
		ptos_command_tail(tail, 2, args);
		const char env[] = "DEMO=present\0ARGV=NULL:2\0cli.tos\0two words\0 \0\0";
		ptos_allocator_init();
		int32_t bp = ptos_load(argv[1], tail, env);
		assert(bp > 0);
		int32_t status = ptos_run_program(bp);
		fprintf(stderr, "status=%ld, output=%s\n", (long)status, output);
		assert(status == 37 && strstr(output, "CLI fixture passed"));
		ptos_allocator_destroy(); ptos_memory_destroy();
		return 0;
	}
	test_memory();
	test_arguments();
	test_delegation();
	test_guest_super();
	test_overlay(4); test_overlay(6);
	test_bad_images();
	test_nested_supervisor_stack();
	test_fault();
	test_cli();
	ptos_allocator_destroy();
	assert(ptos_alloc(-1) == PTOS_RAM_END - PTOS_ALLOC_START);
	ptos_memory_destroy();
	remove("contract.prg");
	puts("pTOS backend contract: PASS");
	return 0;
}
