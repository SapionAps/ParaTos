#include <stdio.h>

#include "common.h"
#include "gemdos.h"
#include "runtime.h"

void InitGuestCPU(emuptr32_t basepage)
{
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68020);
	m68k_pulse_reset();

	uint32_t hitpa = m68k_read_field(basepage, basepage_t, p_hitpa);
	uint32_t tbase = m68k_read_field(basepage, basepage_t, p_tbase);
	uint32_t stack = hitpa;

	stack-=4;
	m68k_write_memory_32(stack, basepage);
	stack-=4;
	m68k_write_memory_32(stack, 0);

	m68k_set_reg(M68K_REG_SP, basepage-4);
	m68k_set_reg(M68K_REG_SR, 0x0300);
	m68k_set_reg(M68K_REG_SP, stack);
	m68k_set_reg(M68K_REG_PC, tbase);
}

void DumpGuestFault(FILE *out)
{
	static const char* regs[] = {
		"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
		"A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
		"PC", "SR", "SP", "USP", "ISP", "MSP", "SFC", "DFC", "VBR", "CACR", "CAAR",
		"PREF_ADDR", "PREF_DATA", "PPC", "IR"};
	for(int i=M68K_REG_D0; i<M68K_REG_CPU_TYPE; i++)
	{
		fprintf(out, "%-4s= %08x  ", regs[i], m68k_get_reg(NULL, i));
		if ((i%4)==3) fprintf(out, "\n");
	}
	fprintf(out, "\n");
	emuptr32_t pc =	m68k_get_reg(NULL, M68K_REG_PPC);
	char buf[128];
	fprintf(out, "Caught SEGV while executing instruction at %08x -> %08x\n", pc, m68k_get_reg(NULL, M68K_REG_PC));
	emuptr32_t earlier = pc;
	for(int i=0; i<20; i++)
	{
		bool found = false;
		for (int j=24; j>0; j-=2)
		{
			if((unsigned)j == m68k_disassemble(buf, earlier-j, M68K_CPU_TYPE_68020) )
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
		fprintf(out, "%s%08x %-32s", address==pc?"-> ":"   ", address, buf);
		for(int j=0; j<width; j+=2)
			fprintf(out, "%04x ", m68k_read_memory_16(address+j));
		if (width == 2)
			fprintf(out, "          ");
		if (width == 4)
			fprintf(out, "     ");
		for(int j=0; j<width; j+=1)
		{
			char c=memory[address+j];
			fprintf(out, "%c", (c>' '&&c<='~')?c:'.');
		}
		fprintf(out, "\n");
	}
}
