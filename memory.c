#include "memory.i"

size_t memory_sz;
uint8_t* memory;

unsigned int m68k_read_disassembler_8(unsigned int address)
{
	return MEM8(address);
}

unsigned int m68k_read_disassembler_16 (unsigned int address)
{
	return paratos_be16toh(MEM16(address));
}

unsigned int m68k_read_disassembler_32 (unsigned int address)
{
	return paratos_be64toh(MEM64(address));
}

uint32_t m68k_write_string(uint32_t address, const char* value, uint32_t maxLen)
{
	uint32_t i = 0;
	char *buffer = (char*)&memory[address];
	for(i=0; i<maxLen; i++)
	{
		buffer[i] = value[i];
		if (value[i] == 0)
			break;
	}
	if(buffer[i] != 0)
		buffer[++i] = 0;
	return i;
}

void  m68k_read_array8(uint32_t address, char* dest, uint32_t size)
{
	memcpy(dest, &memory[address], size);
}

void m68k_write_array8(uint32_t address, const char* value, uint32_t size)
{
	memcpy(&memory[address], value, size);
}

void m68k_read_to_64(uint32_t address, uint64_t* dest)
{
	*dest = m68k_read_memory_64(address);
}
void m68k_read_to_32(uint32_t address, uint32_t* dest)
{
	*dest = m68k_read_memory_32(address);
}
void m68k_read_to_16(uint32_t address, uint16_t* dest)
{
	*dest = m68k_read_memory_16(address);
}
void m68k_read_to_8(uint32_t address, uint8_t* dest)
{
	*dest = m68k_read_memory_8(address);
}
