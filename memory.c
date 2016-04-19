#include <stdint.h>
uint32_t memory_sz;
uint8_t* memory;

#define memory16 ((uint16_t*)memory)
#define memory32 ((uint32_t*)memory)
#define MEM8(address) (memory[(address) ^ 0b11])
#define MEM16(address) (memory16[((address) >> 1) ^ 0b1])
#define MEM32(address) (memory32[(address) >> 2])

unsigned int m68k_read_memory_8(unsigned int address)
{
	return MEM8(address);
}

unsigned int m68k_read_memory_16(unsigned int address)
{
	return MEM16(address);
}

unsigned int m68k_read_memory_32(unsigned int address)
{
	if ((address & 2) == 0)
		return MEM32(address);
	else
		return (uint32_t)MEM16(address)<<16 | MEM16(address+2);
}

uint64_t m68k_read_memory_64(unsigned int address)
{
	return (((uint64_t)m68k_read_memory_32(address))<<32) | m68k_read_memory_32(address+4);
}

unsigned int m68k_read_disassembler_8(unsigned int address)
{
	return MEM8(address);
}

unsigned int m68k_read_disassembler_16 (unsigned int address)
{
	return MEM16(address);
}

unsigned int m68k_read_disassembler_32 (unsigned int address)
{
	if ((address & 2) == 0)
		return MEM32(address);
	else
		return (uint32_t)MEM16(address)<<16 | MEM16(address+2);
}

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
	MEM8(address) = (uint8_t)value;
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
	MEM16(address) = (uint16_t)value;
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
	if ((address & 2) == 0)
		MEM32(address) = (uint32_t)value;
	else
	{
		MEM16(address) = value >>16;
		MEM16(address+2) = value & 0xffff;
	}
}

void m68k_write_memory_64(unsigned int address, uint64_t value)
{
	m68k_write_memory_32(address, value >> 32);
	m68k_write_memory_32(address+4, value & 0xffffffff);
}

uint32_t m68k_read_string(uint32_t address, char* dest, uint32_t maxLen, int is_path)
{
	uint32_t i = 0;
	for(i=0; i<maxLen; i++)
	{
		dest[i] = m68k_read_memory_8(address+i);
		if (dest[i] == 0)
			break;
		if (is_path && dest[i] == '\\')
			dest[i] = '/';
	}
	if(dest[i] != 0)
		dest[++i] = 0;
	return i;
}
