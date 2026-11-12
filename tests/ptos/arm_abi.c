#include <stdint.h>
#include "host.h"
#include "vdi.h"

/* Link native.c, arm/call.c and arm/vdi.c, not reimplemented bridges. */
static unsigned int stage;
static paratos_vdi_control vdi_control = {.code = VDI_OPEN, .nintin = 11};
static int16_t vdi_input[11] = {[10] = 2}, vdi_output[45], vdi_points[12];
static paratos_vdi_pb vdi_block = {&vdi_control, vdi_input, 0, vdi_output, vdi_points};

static int semihost(unsigned int op, const void *argument)
{
	register unsigned int r0 __asm__("r0") = op;
	register const void *r1 __asm__("r1") = argument;
	__asm__ volatile("svc #0x123456" : "+r"(r0), "+r"(r1) : : "memory", "cc");
	return r0;
}

static _Noreturn void finish(int failed)
{
	const uint32_t exit_block[2] = {0x20026, (uint32_t)failed};
	semihost(4, failed ? "ARM pTOS SVC ABI: FAIL\n" : "ARM pTOS SVC ABI: PASS\n");
	semihost(0x20, exit_block);
	for (;;) { }
}

void arm_abi_failure(void) { finish(1); }

int32_t arm_abi_dispatch(uint32_t *registers, uint32_t saved_status)
{
	static const uint32_t expected[][7] = {
		{0x154, 0xfffffffe, 0x41001234, 0x89abcdef, 0x76543210, 0, 0},
		{0x17e, PTOS_RAM_START, PTOS_RAM_END - PTOS_RAM_START, 3, 0x100022, 0xffffffff, 0},
		{0x17f, PTOS_RAM_START, PTOS_RAM_END - PTOS_RAM_START, 0, 0, 0, 0}
	};
	if ((saved_status & 0x1f) != 0x10) finish(1); /* must originate in USR */
	unsigned int trap = *(const uint32_t *)(uintptr_t)(registers[13] - 4) & 0xffffff;
	if (registers[0] == 0xfffe) finish(trap != 1 || stage != 7 || registers[1] != 0);
	if (stage >= 4 && stage <= 6)
	{
		if (trap != 2 || registers[0] != 0x73 || registers[1] != (uintptr_t)&vdi_block) finish(1);
		if (vdi_block.control->code != 100 || vdi_block.control->nintin != 11 || vdi_block.intin[10] != 2) finish(1);
		for (unsigned int i = 1; i < 7; i++) registers[i] = 0xdead0000 + i;
		if (stage++ == 4) return -32;
		if (stage == 6) return (int32_t)0x89abcdef;
		vdi_control.handle = 0x1234; vdi_control.nintout = 1; vdi_control.nptsout = 1;
		vdi_output[0] = 0x5678; vdi_points[0] = -123; vdi_points[1] = 456;
		return 0;
	}
	if (stage == 3)
	{
		if (trap != 13 || registers[0] != BIOS_OP_BCONOUT || registers[1] != 2 || registers[2] != 'B') finish(1);
		for (unsigned int i = 1; i < 7; i++) registers[i] = 0xdead0000 + i;
		stage++; return -36;
	}
	if (trap != 1) finish(1);
	if (stage >= 3) finish(1);
	for (unsigned int i = 0; i < 7; i++)
		if (registers[i] != expected[stage][i]) finish(1);
	/* Check that compiler-visible clobbers do not corrupt the calling C
	 * function's callee-saved state. pTOS may change argument registers. */
	for (unsigned int i = 1; i < 7; i++) registers[i] = 0xdead0000 + i;
	stage++;
	return stage == 1 ? -36 : stage == 2 ? (int32_t)PTOS_RAM_START : 0;
}

void arm_abi_user(void)
{
	int failed = ptos_call(GEMDOS_OP_SSYSTEM, -2, 0x41001234, (intptr_t)0x89abcdef, 0x76543210) != -36;
	failed |= ptos_map(PTOS_RAM_START, PTOS_RAM_END - PTOS_RAM_START) != PTOS_RAM_START;
	failed |= ptos_unmap(PTOS_RAM_START, PTOS_RAM_END - PTOS_RAM_START) != 0;
	failed |= ptos_bios_call(BIOS_OP_BCONOUT, 2, 'B') != -36;
	failed |= ptos_native_vdi(&vdi_block) != -32;
	failed |= ptos_native_vdi(&vdi_block) != (int32_t)0x89abcdef;
	failed |= ptos_native_vdi(&vdi_block) != 0;
	failed |= vdi_control.handle != 0x1234 || vdi_control.nintout != 1 || vdi_control.nptsout != 1;
	failed |= vdi_output[0] != 0x5678 || vdi_points[0] != -123 || vdi_points[1] != 456;
	ptos_call(0xfffe, failed, 0, 0, 0);
	for (;;) { }
}
