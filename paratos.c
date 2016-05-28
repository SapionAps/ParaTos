#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <setjmp.h>
#include <signal.h>

#include "common.h"
#include "m68k.h"
#include "m68kcpu.h"
#include "gemdos.h"
#include "loader.h"
#include "sysvars.h"
#include "cookiejar.h"

void dispatch_gem_trap()
{
	uint32_t pc = m68k_get_reg(NULL, M68K_REG_PC);
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
    uint16_t num = m68k_read_memory_16(sp);
//	uint32_t ad0 = m68k_get_reg(NULL, M68K_REG_D0);
//  uint32_t ad1 = m68k_get_reg(NULL, M68K_REG_D1);

	//printf("GEM\n");

	printf("AES sp=0x%08x pc=0x%08x\n", sp, pc);

	m68k_set_reg(M68K_REG_D0, -(int)num);
}

void dispatch_bios_trap()
{
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
	uint16_t num = m68k_read_memory_16(sp);

	printf("BIOS(0x%02x)\n", num);

	m68k_set_reg(M68K_REG_D0, -(int)num);
}


void dispatch_xbios_trap()
{
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
    int16_t num = m68k_read_memory_16(sp);
	int16_t retval = -num;
	switch(num)
	{
		case 4: // getrez
		retval = 3; // ST Mono
		break;
		case 17: // random
		retval = rand() & 0x00ffffff;
		break;
		case 38: // Supexec
		// We're just going to fake it an run it in user mode anyway
		{
			uint32_t call = m68k_read_memory_32(sp+2);
			uint32_t callee = m68k_get_reg(NULL, M68K_REG_PC);
			m68k_write_memory_32(sp-4, callee);
			m68k_set_reg(M68K_REG_SP, sp-4);
			m68k_set_reg(M68K_REG_PC, call);
			return;
		}
		default:
		printf("XBIOS(0x%02x)\n", num);
	}

    m68k_set_reg(M68K_REG_D0, retval);
}

emuptr32_t current_process;
char* paratos_exe;

void FindExecutable(void)
{
	size_t pathSize = 1024;
	while(true) {
		paratos_exe = malloc(pathSize);
		if(paratos_exe == NULL)
		{
			fprintf(stderr, "malloc: Out of memory.\n");
			exit(1);
		}

		int r = readlink("/proc/self/exe", paratos_exe, pathSize);
		if (r == -1)
		{
			perror("readlink(\"/proc/self/exe\")");
			exit(1);
		}
		else if (r < pathSize)
		{
			return;
		}
		else
		{
			free(paratos_exe);
			pathSize *= 2;
		}
	}
}

static sigjmp_buf return_from_signal_handler;
void handle_segv(int signum, siginfo_t * info, void * p)
{
	printf("SIGSEGV Addr: %p (ST mem: %lx)\n", info->si_addr, info->si_addr-(void*)memory);
	siglongjmp(return_from_signal_handler, signum);
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <program.tos> [arguments...]\n", argv[0]);
		return 1;
	}

	FindExecutable();
	InitM68KMemory();
	InitMemory();
	InitCookieJar();

	current_process = LoadExe(argv[1], argv+1, argc-1);
	if (current_process == 0)
	{
		fprintf(stderr, "error: cannot load %s.\n", argv[1]);
		return 1;
	}

	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	m68k_pulse_reset();

	uint32_t hitpa = m68k_read_field(current_process, basepage_t, p_hitpa);
	uint32_t tbase = m68k_read_field(current_process, basepage_t, p_tbase);
	uint32_t stack = hitpa;

	stack-=4;
	m68k_write_memory_32(stack, current_process);
	stack-=4;
	m68k_write_memory_32(stack, 0);

	m68k_set_reg(M68K_REG_SP, current_process-4);
	m68k_set_reg(M68K_REG_SR, 0x0300);
	m68k_set_reg(M68K_REG_SP, stack);
	m68k_set_reg(M68K_REG_PC, tbase);

	struct sigaction act_segv;
	act_segv.sa_sigaction = handle_segv;
	act_segv.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &act_segv, NULL);
	for (;;)
	{
		int ret = sigsetjmp(return_from_signal_handler, 1);
		if (!ret)
		{
			m68k_execute(10000);
		}
		else
		{
			printf("longjmp from exception handler returned %d\n", ret);
			// Handle return from signal handler
			if (ret == SIGSEGV)
			{
				printf("Trying to handle SEGV...\n");
				m68ki_exception_address_error();
			}
		}
	}

	return 0;
}
