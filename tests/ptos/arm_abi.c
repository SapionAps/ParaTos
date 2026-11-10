#include <stdint.h>
#include "host.h"

/* Link native.c and arm/call.c, not a reimplementation of the bridge. */
static unsigned int stage;

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
	if ((*(const uint32_t *)(uintptr_t)(registers[13] - 4) & 0xffffff) != 1) finish(1);
	if (registers[0] == 0xfffe) finish(stage != 3 || registers[1] != 0);
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
	ptos_call(0xfffe, failed, 0, 0, 0);
	for (;;) { }
}
