#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include "m68k.h"
#include "host.h"
#include "memory.h"
#include "guest.h"

static jmp_buf fault_return;
static int running;

_Noreturn void ptos_fault(uint32_t address, const char *reason)
{
	fprintf(stderr, "Paratos: %s at guest address %08lx\n", reason, (unsigned long)address);
	if (running) fprintf(stderr, "guest PC=%08lx SP=%08lx SR=%04lx\n",
		(unsigned long)m68k_get_reg(NULL, M68K_REG_PC),
		(unsigned long)m68k_get_reg(NULL, M68K_REG_SP),
		(unsigned long)m68k_get_reg(NULL, M68K_REG_SR));
	if (running) longjmp(fault_return, 1);
	exit(1);
	/* Some libcmini headers do not annotate exit() as noreturn. */
	__builtin_unreachable();
}

int32_t ptos_run_program(uint32_t bp)
{
	ptos_process_cleanup();
	ptos_finished = 0; ptos_status = 0;
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	m68k_pulse_reset();
	ptos_process = bp;
	ptos_take_image(bp);
	ptos_start_cpu(bp);
	running = 1;
	if (setjmp(fault_return)) ptos_status = 255;
	else while (!ptos_finished) m68k_execute(10000);
	running = 0;
	ptos_process_cleanup();
	return ptos_status;
}
