#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "byteorder.h"
#include "m68k.h"
#include "m68kcpu.h"
#include "gdos.h"
#include "loader.h"

extern char **environ;

#define MAX_TOS_ARG 126

void BuildTosArglist(uint32_t args, char* argv[], int argc)
{
	int n = 1;
    for (int i = 0; i < argc; ++i)
    {
        if (i > 0)
		{
			m68k_write_memory_8(args+(n++), ' ');
			if (n>MAX_TOS_ARG)
				goto trunc;
		}
		for(char* c = argv[i]; *c; c++)
		{
			m68k_write_memory_8(args+(n++), *c);
			if (n>MAX_TOS_ARG)
				goto trunc;
		}
    }
trunc:
	m68k_write_memory_8(args+n, '\0');
    m68k_write_memory_8(args, n-1);
}



basepage_t* LoadExe(const char* filename, char* argv[], int argc)
{
	tos_header_t header;
	FILE* file = fopen(filename,"r");
	if (!file)
	{
		return 0;
	}
	fread(&header, sizeof(tos_header_t), 1, file);
	header.PRG_tsize = be32toh(header.PRG_tsize);
	header.PRG_dsize = be32toh(header.PRG_dsize);
	header.PRG_bsize = be32toh(header.PRG_bsize);
	header.PRG_ssize = be32toh(header.PRG_ssize);
	header.PRG_res1 = be32toh(header.PRG_res1);
	header.PRGFLAGS = be32toh(header.PRGFLAGS);
	header.ABSFLAG = be16toh(header.ABSFLAG);
	printf("Reading in text size: %d, data: %d, magic: %04x\n", header.PRG_tsize, header.PRG_dsize, header.PRG_magic);

	uint32_t base_tpa = Malloc(Malloc(-1));

	// Copy command line
	uint32_t command_line = base_tpa+offsetof(basepage_t, p_cmdlin);
	BuildTosArglist(command_line, argv, argc);

	// Base page is long word aligned; all long entries are pre-byeswapped in memory
	basepage_t* basepage = (basepage_t*)(memory + base_tpa);
	basepage->p_lowtpa = base_tpa;
	basepage->p_hitpa = memory_sz;

	uint32_t current = base_tpa + sizeof(basepage_t);
	basepage->p_env = current;          /* Address of the environment string   */

	// copy environment
	for(char ** ep=environ; *ep; ep++)
	{
		for(char *vp=*ep; *vp;vp++)
		{
			m68k_write_memory_8(current++, *vp);
		}
		m68k_write_memory_8(current++, 0);

	}
	m68k_write_memory_8(current++, 0);
	while((current & 3) !=0)
		current++;

	// Set up rest of base page (offset is 32 bit aligned, so we can access the 32 bit values directly)
	basepage->p_tbase = current;        /* Start address of the program code   */
	basepage->p_tlen = header.PRG_tsize;        /* Length of the program code          */
	current += header.PRG_tsize;
	basepage->p_dbase = current;        /* Start address of the DATA segment   */
	basepage->p_dlen = header.PRG_dsize;         /* Length of the DATA section          */
	current += header.PRG_dsize;
	basepage->p_bbase = current;        /* Start address of the BSS segment    */
	basepage->p_blen = header.PRG_bsize;         /* Length of the BSS section           */
	current += header.PRG_bsize;
	basepage->p_dta = command_line;          /* Pointer to the default DTA          */
							 /* Warning: Points first to the        */
							 /* command line !                      */
	basepage->p_parent = 0;       /* Pointer to the basepage of the      */
							 /* calling processes                   */

	printf("Reading in text and data from executable: %d+%d at address %d\n", basepage->p_tlen, basepage->p_dlen, basepage->p_tbase);
	for(int i=basepage->p_tbase; i<basepage->p_bbase; i++)
	{
		uint8_t byte;
		if(!fread(&byte,sizeof(char),1, file))
		{
			return 0;
		}
		m68k_write_memory_8(i, byte);
	}
	printf("Skipping %d bytes of symbol data\n", header.PRG_ssize);
	fseek(file, header.PRG_ssize, SEEK_CUR);
	uint8_t delta=0;
	uint32_t fixup=0;
	uint32_t unfixed;
	uint32_t fixed;
	if (fread(&fixup, sizeof(uint32_t), 1, file))
	{
		fixup = be32toh(fixup);
		printf("Performing fixups starting at offset %08x\n", fixup);
		if (fixup)
		{
			fixup += basepage->p_tbase;
			do {
				unfixed = m68k_read_memory_32(fixup);
				fixed = unfixed + basepage->p_tbase;

				//printf("Fixing up %08x->%08x at address %08x\n",unfixed, fixed, fixup);
				m68k_write_memory_32(fixup, fixed);
				delta=0;
				do {
					fread(&delta, sizeof(uint8_t), 1, file);
					fixup += delta == 1?254:delta;
				} while(delta==1);
			} while(delta != 0);
		}
	}

	return basepage;
}
