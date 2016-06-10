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

void dispatch_line_f()
{
	fprintf(stderr, "FPU is not emulated\n");
}

void dispatch_line_a()
{
	uint32_t pc = m68k_get_reg(NULL, M68K_REG_PC);
	uint16_t ins = m68k_get_reg(NULL, M68K_REG_IR);
	printf("LINE-A %04x pc=0x%08x\n", ins, pc);
	m68k_set_reg(M68K_REG_D0, 0);
	m68k_set_reg(M68K_REG_A0, 0);
	m68k_set_reg(M68K_REG_A1, 0);
	m68k_set_reg(M68K_REG_A2, 0);
}

void dispatch_gem_trap()
{
	uint32_t op = m68k_get_reg(NULL, M68K_REG_D0);
	emuptr32_t pblock = m68k_get_reg(NULL, M68K_REG_D1);

	emuptr32_t ctrl = m68k_read_memory_32(pblock);

	switch (op) {
		case 0x73:
		{
			uint16_t vdiop = m68k_read_memory_16(ctrl);
			uint16_t ptsinc = m68k_read_memory_16(ctrl+2);
			uint16_t intinc = m68k_read_memory_16(ctrl+6);
			uint16_t subop = m68k_read_memory_16(ctrl+10);
			uint16_t handle = m68k_read_memory_16(ctrl+12);
			m68k_write_memory_16(ctrl+4, 0);
			m68k_write_memory_16(ctrl+8, 0);
			m68k_write_memory_16(ctrl+12, 0);
			printf("VDI pblock=%08x op=%d,%d handle=%d, #pt=%d, #int=%d\n", pblock, vdiop, subop, handle, ptsinc, intinc);
			break;
		}
		case 0xc8:
		{
			uint16_t aesop = m68k_read_memory_16(ctrl);
			emuptr32_t global = m68k_read_memory_32(pblock+4);
			emuptr32_t intin = m68k_read_memory_32(pblock+8);
			emuptr32_t intout = m68k_read_memory_32(pblock+12);
			emuptr32_t adrin = m68k_read_memory_32(pblock+16);
			emuptr32_t adrout = m68k_read_memory_32(pblock+20);
			switch (aesop) {
				case 120: // shel_read
				m68k_write_memory_16(intout+0, 1);
				emuptr32_t cmd = m68k_read_memory_32(adrin+0);
				emuptr32_t cmdln = m68k_read_memory_32(adrin+4);
				printf("AES pblock=%0x, op=shel_read/%d (%x,%x)\n", pblock, aesop, cmd, cmdln);
				m68k_write_memory_32(cmd, m68k_read_field(current_process, basepage_t, p_env));
				m68k_write_memory_32(cmdln, current_process + offsetof(basepage_t, p_cmdlin));
				printf("%s %s\n", &memory[m68k_read_memory_32(cmd)], &memory[m68k_read_memory_32(cmdln)]);
				break;
				default:
				printf("AES pblock=%0x, op=%d\n", pblock, aesop);
				break;

			}
			break;
		}
		default:
		printf("Unknown trap#2 pblock=%0x\n", pblock);
		break;
	}
}

void dispatch_bios_trap()
{
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
	uint16_t num = m68k_read_memory_16(sp);

	if (num == 3)
	{
		printf("BCONOUT(%d, %c)\n", m68k_read_memory_16(sp-2), 0x7f&m68k_read_memory_16(sp-4));
	}
	else {
		printf("BIOS(0x%02x)\n", num);
	}


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
	struct sigaction old_segv;
	act_segv.sa_sigaction = handle_segv;
	act_segv.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &act_segv, &old_segv);
	for (;;)
	{
		int ret = sigsetjmp(return_from_signal_handler, 1);
		if (!ret)
		{
			m68k_execute(10000);
		}
		else
		{
			static const char* regs[] = {
				"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
				"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
				"PC", "SR", "SP", "USP", "ISP", "MSP", "SFC", "DFC", "VBR", "CACR", "CAAR",
				"PREF_ADDR", "PREF_DATA", "PPC", "IR"};
			printf("longjmp from exception handler returned %d\n", ret);
			// Handle return from signal handler
			if (ret == SIGSEGV)
			{
				sigaction(SIGSEGV, &old_segv, NULL);
				for(int i=M68K_REG_D0; i<M68K_REG_CPU_TYPE; i++)
				{
					printf("%-4s= %08x  ", regs[i], m68k_get_reg(NULL, i));
					if ((i%4)==3) printf("\n");
				}
				printf("\n");
				emuptr32_t pc =	m68k_get_reg(NULL, M68K_REG_PPC);
				char buf[128];
				printf("Caught SEGV while executing instruction at %08x -> %08x\n", pc, m68k_get_reg(NULL, M68K_REG_PC));
				emuptr32_t earlier = pc;
				for(int i=0; i<20; i++)
				{
					bool found = false;
					for (int j=24; j>0; j-=2)
					{
						if(j == m68k_disassemble(buf, earlier-j, M68K_CPU_TYPE_68020) )
						{
							found = true;
							earlier -= j;
							break;
						}
					}
					if(!found)
						break;
				}
				int width = 0;
				for(emuptr32_t address = earlier ; address <= pc+32; address+=width)
				{
					width=m68k_disassemble(buf, address, M68K_CPU_TYPE_68020);
					printf("%s%08x %-32s", address==pc?"-> ":"   ", address, buf);
					for(int j=0; j<width; j+=2)
						printf("%04x ", m68k_read_memory_16(address+j));
					if (width == 2)
						printf("          ");
					if (width == 4)
						printf("     ");
					for(int j=0; j<width; j+=1)
					{
						char c=memory[address+j];
						printf("%c", (c>' '&&c<='~')?c:'.');
					}
					printf("\n");
				}
				exit(-1);
				// m68ki_exception_address_error();
			}
		}
	}

	return 0;
}
