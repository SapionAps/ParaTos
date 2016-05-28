#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "common.h"
#include "byteorder.h"
#include "m68k.h"
#include "m68kcpu.h"
#include "gemdos.h"
#include "loader.h"

extern char **environ;

#define MAX_TOS_ARG 126

uint32_t BuildTosArglist(uint32_t args, char* argv[], int argc, uint32_t env)
{
	int n = 1;
	for (int i = 1; i < argc; ++i)
	{
		if (i > 0)
		{
			m68k_write_memory_8(args+(n++), ' ');
			if (n>MAX_TOS_ARG)
				goto trunc;
		}
		if(argv[i][0]==0)
		{
			m68k_write_memory_8(args+(n++), '\'');
			if (n>MAX_TOS_ARG)
			{
				n--;
				goto trunc;
			}
			m68k_write_memory_8(args+(n++), '\'');
			if (n>MAX_TOS_ARG)
				goto trunc;

		}
		else
		{
			for(char* c = argv[i]; *c; c++)
			{
				m68k_write_memory_8(args+(n++), *c);
				if (n>MAX_TOS_ARG)
					goto trunc;
			}
		}
    }
	m68k_write_memory_8(args, n-1);
	goto notrunc;
trunc:
	m68k_write_memory_8(args+n, '\0');
	m68k_write_memory_8(args, 127);
notrunc:

	// ARGV procedure - pass complete command line at the end of the env string
	env+=m68k_write_string(env, "ARGV=", 5);
	// Check for empty parameters and enable extended ARGV procedure in that case
	bool nullAdded = false;
	char buffer[21]; // buffer for sprintf
	for (int i = 0; i < argc; ++i)
	{
		if(argv[i][0]==0)
		{
			snprintf(buffer, 21, nullAdded?",%d":"NULL:%d", i);
			nullAdded = true;
			env+=m68k_write_string(env, buffer, 20);
		}
	}
	m68k_write_memory_8(env++, '\0');
	for (int i = 0; i < argc; ++i)
	{
		if(argv[i][0]==0)
		{
			m68k_write_memory_8(env++, ' ');
		}
		else
		{
			env+=m68k_write_string(env, argv[i], 0x7fffffff);
		}
		m68k_write_memory_8(env++, '\0');
	}


	return env;
}


#define WRITE_BASEPAGE(field,value) m68k_write_field(base_tpa, basepage_t, field, value)
#define READ_BASEPAGE(field) m68k_read_field(base_tpa, basepage_t, field)

emuptr32_t LoadExe(const char* filename, char* argv[], int argc)
{
	tos_header_t header;
	FILE* file = fopen(filename,"r");
	if (!file)
	{
		perror(filename);
		return 0;
	}
	fread(&header, sizeof(tos_header_t), 1, file);
	header.PRG_magic = be16toh(header.PRG_magic);
	header.PRG_tsize = be32toh(header.PRG_tsize);
	header.PRG_dsize = be32toh(header.PRG_dsize);
	header.PRG_bsize = be32toh(header.PRG_bsize);
	header.PRG_ssize = be32toh(header.PRG_ssize);
	header.PRG_res1 = be32toh(header.PRG_res1);
	header.PRGFLAGS = be32toh(header.PRGFLAGS);
	header.ABSFLAG = be16toh(header.ABSFLAG);
	printf("Reading in text size: %d, data: %d, magic: %04x\n", header.PRG_tsize, header.PRG_dsize, header.PRG_magic);

	uint32_t base_tpa = Malloc(Malloc(-1));

	WRITE_BASEPAGE(p_lowtpa, base_tpa);
	WRITE_BASEPAGE(p_hitpa, memory_sz);

	uint32_t current = base_tpa + sizeof(basepage_t);
	WRITE_BASEPAGE(p_env, current);          /* Address of the environment string   */

	// copy environment
	for(char ** ep=environ; *ep; ep++)
	{
		for(char *vp=*ep; *vp;vp++)
		{
			m68k_write_memory_8(current++, *vp);
		}
		m68k_write_memory_8(current++, 0);

	}
	// Copy command line
	uint32_t command_line = base_tpa+offsetof(basepage_t, p_cmdlin);
	current = BuildTosArglist(command_line, argv, argc, current);

	m68k_write_memory_8(current++, 0);
	while((current & 3) !=0)
		current++;

	// Set up rest of base page (offset is 32 bit aligned, so we can access the 32 bit values directly)
	WRITE_BASEPAGE(p_tbase, current);        /* Start address of the program code   */
	WRITE_BASEPAGE(p_tlen, header.PRG_tsize);        /* Length of the program code          */
	current += header.PRG_tsize;
	WRITE_BASEPAGE(p_dbase, current);        /* Start address of the DATA segment   */
	WRITE_BASEPAGE(p_dlen, header.PRG_dsize);         /* Length of the DATA section          */
	current += header.PRG_dsize;
	WRITE_BASEPAGE(p_bbase, current);        /* Start address of the BSS segment    */
	WRITE_BASEPAGE(p_blen, header.PRG_bsize);         /* Length of the BSS section           */
	current += header.PRG_bsize;
	WRITE_BASEPAGE(p_dta, command_line);          /* Pointer to the default DTA          */
							 /* Warning: Points first to the        */
							 /* command line !                      */
	WRITE_BASEPAGE(p_parent, 0);       /* Pointer to the basepage of the      */
							 /* calling processes                   */

	printf("Reading in text and data from executable: %d+%d at address %d\n", READ_BASEPAGE(p_tlen), READ_BASEPAGE(p_dlen), READ_BASEPAGE(p_tbase));
	char* dest = memory+READ_BASEPAGE(p_tbase);
	if(!fread(dest, sizeof(char), READ_BASEPAGE(p_tlen)+READ_BASEPAGE(p_dlen), file))
	{
		return 0;
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
			fixup += READ_BASEPAGE(p_tbase);
			do {
				unfixed = m68k_read_memory_32(fixup);
				fixed = unfixed + READ_BASEPAGE(p_tbase);

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

	return base_tpa;
}
