#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "gemdos.h"

void dispatch_line_f(void)
{
	PosixLog(2, "FPU is not emulated\n");
}

void dispatch_line_a(void)
{
	uint32_t pc = m68k_get_reg(NULL, M68K_REG_PC);
	uint16_t ins = m68k_get_reg(NULL, M68K_REG_IR);
	PosixLog(1,"LINE-A %04x pc=0x%08x\n", ins, pc);
	m68k_set_reg(M68K_REG_D0, 0);
	m68k_set_reg(M68K_REG_A0, 0);
	m68k_set_reg(M68K_REG_A1, 0);
	m68k_set_reg(M68K_REG_A2, 0);
}

void dispatch_gem_trap(void)
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
			PosixLog(1,"VDI pblock=%08x op=%d,%d handle=%d, #pt=%d, #int=%d\n", pblock, vdiop, subop, handle, ptsinc, intinc);
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
			(void)global;
			(void)intin;
			(void)adrout;
			switch (aesop) {
				case 120: // shel_read
				m68k_write_memory_16(intout+0, 1);
				emuptr32_t cmd = m68k_read_memory_32(adrin+0);
				emuptr32_t cmdln = m68k_read_memory_32(adrin+4);
				PosixLog(1,"AES pblock=%0x, op=shel_read/%d (%x,%x)\n", pblock, aesop, cmd, cmdln);
				m68k_write_memory_32(cmd, m68k_read_field(current_process, basepage_t, p_env));
				m68k_write_memory_32(cmdln, current_process + offsetof(basepage_t, p_cmdlin));
				PosixLog(1,"%s %s\n", &memory[m68k_read_memory_32(cmd)], &memory[m68k_read_memory_32(cmdln)]);
				break;
				default:
				PosixLog(1,"AES pblock=%0x, op=%d\n", pblock, aesop);
				break;

			}
			break;
		}
		default:
		PosixLog(1,"Unknown trap#2 pblock=%0x\n", pblock);
		break;
	}
}

void dispatch_bios_trap(void)
{
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
	uint16_t num = m68k_read_memory_16(sp);

	if (num == 3)
	{
		PosixLog(1,"BCONOUT(%d, %c)\n", m68k_read_memory_16(sp-2), 0x7f&m68k_read_memory_16(sp-4));
	}
	else {
		PosixLog(1,"BIOS(0x%02x)\n", num);
	}


	m68k_set_reg(M68K_REG_D0, -(int)num);
}


void dispatch_xbios_trap(void)
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
		PosixLog(1,"XBIOS(0x%02x)\n", num);
	}

    m68k_set_reg(M68K_REG_D0, retval);
}

