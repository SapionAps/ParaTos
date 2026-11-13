#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gemdos.h"
#include "host.h"
#include "loader.h"
#include "byteorder.h"
#include "tasks.h"

static int guest_range(uint32_t address, uint32_t size)
{
	return address < memory_sz && size <= memory_sz - address;
}

int32_t PosixEnvironmentSize(uint32_t env)
{
	if (env >= memory_sz || memory_sz - env < 2) return TOS_EINVAL;
	for (uint32_t n = 0; n + 1 < memory_sz - env; n++)
		if (!memory[env + n] && !memory[env + n + 1]) return (int32_t)(n + 2);
	return TOS_EINVAL;
}

int32_t PosixCreateBasepage(emuptr32_t tail, emuptr32_t env)
{
	if (tail && !guest_range(tail, 128)) return TOS_EINVAL;
	if (!env) env = m68k_read_field(current_process, basepage_t, p_env);
	uint32_t length = 2;
	if (env)
	{
		if (!guest_range(env, 2)) return TOS_EINVAL;
		length = 0;
		while (guest_range(env, length + 2))
		{
			if (!memory[env + length] && !memory[env + length + 1]) break;
			length++;
		}
		if (!guest_range(env, length + 2)) return TOS_EINVAL;
		length += 2;
	}
	emuptr32_t copy = Malloc((int32_t)length);
	if (!copy) return TOS_ENSMEM;
	if (env) memcpy(memory + copy, memory + env, length);
	else memset(memory + copy, 0, length);
	int32_t size = (int32_t)Malloc(-1);
	if (size < (int32_t)sizeof(basepage_t) + 8) { Mfree(copy); return TOS_ENSMEM; }
	emuptr32_t bp = Malloc(size);
	if (!bp) { Mfree(copy); return TOS_ENSMEM; }
	MarkBasepage(bp);
	memset(memory + bp, 0, (size_t)size);
	m68k_write_field(bp, basepage_t, p_lowtpa, bp);
	m68k_write_field(bp, basepage_t, p_hitpa, bp + size);
	m68k_write_field(bp, basepage_t, p_dta, bp + offsetof(basepage_t, p_cmdlin));
	m68k_write_field(bp, basepage_t, p_env, copy);
	m68k_write_field(bp, basepage_t, p_parent, current_process);
	if (tail) memcpy(memory + bp + offsetof(basepage_t, p_cmdlin), memory + tail, 128);
	return (int32_t)bp;
}

int32_t PosixLoadImage(const char *path, emuptr32_t tail, emuptr32_t env)
{
	FILE *file = fopen(path, "rb");
	if (!file) return MapErrno();
	int32_t result = TOS_ENOEXEC;
	tos_header_t h;
	if (fread(&h, sizeof(h), 1, file) != 1 || paratos_be16toh(h.PRG_magic) != 0x601a) goto out;
	uint32_t text = paratos_be32toh(h.PRG_tsize);
	uint32_t data = paratos_be32toh(h.PRG_dsize);
	uint32_t bss = paratos_be32toh(h.PRG_bsize);
	uint32_t symbols = paratos_be32toh(h.PRG_ssize);
	uint64_t image_size = (uint64_t)text + data;
	if (!text || image_size + bss + sizeof(basepage_t) + 8 > memory_sz) goto out;
	result = PosixCreateBasepage(tail, env);
	if (result < 0) goto out;
	uint32_t bp = (uint32_t)result, base = bp + sizeof(basepage_t);
	if (image_size + bss + sizeof(basepage_t) + 8 > (uint32_t)MemoryBlockSize(bp))
	{
		result = TOS_ENSMEM;
		goto discard;
	}
	m68k_write_field(bp, basepage_t, p_tbase, base);
	m68k_write_field(bp, basepage_t, p_tlen, text);
	m68k_write_field(bp, basepage_t, p_dbase, base + text);
	m68k_write_field(bp, basepage_t, p_dlen, data);
	m68k_write_field(bp, basepage_t, p_bbase, base + text + data);
	m68k_write_field(bp, basepage_t, p_blen, bss);
	result = TOS_ENOEXEC;
	if (fread(memory + base, 1, (size_t)image_size, file) != image_size) goto discard;
	if (!paratos_be16toh(h.ABSFLAG))
	{
		if (fseek(file, (long)symbols, SEEK_CUR)) goto discard;
		uint32_t offset;
		if (fread(&offset, sizeof(offset), 1, file) != 1) goto discard;
		offset = paratos_be32toh(offset);
		while (offset)
		{
			if ((offset & 1) || (uint64_t)offset + 4 > image_size) goto discard;
			m68k_write_memory_32(base + offset, m68k_read_memory_32(base + offset) + base);
			int delta;
			do {
				delta = fgetc(file);
				if (delta == EOF) goto discard;
				if (delta == 1)
				{
					if ((uint64_t)offset + 254 + 4 > image_size) goto discard;
					offset += 254;
				}
			} while (delta == 1);
			if (!delta) break;
			offset += (uint32_t)delta;
		}
	}
	result = (int32_t)bp;
	goto out;
discard:
	Mfree(m68k_read_field(bp, basepage_t, p_env));
	Mfree(bp);
out:
	fclose(file);
	return result;
}
