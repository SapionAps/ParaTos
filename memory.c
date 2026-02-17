#include "memory.i"

uint32_t memory_sz;
uint8_t* memory;
int page_size;

#if __x86_64__
// Allocate the entire address space addressable by a 68020 processor on 64 bit systems
#define MAX_MEM 0x100000000LL
#else
// ... less on 32 bit (512Mb)
#define MAX_MEM 0x20000000
#endif

void InitM68KMemory(void)
{
	page_size = getpagesize();

	// Reserve entire address space upfront. That way we can grow allocated memory without having to reallocate it
	memory = mmap(NULL, MAX_MEM, PROT_NONE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (memory == MAP_FAILED)
	{
		perror("Mapping 68k memory failed: ");
		exit(1);
	}

	// Actually allocate first 14 Mb of that address space
	memory_sz = 16 * 1024 * 1024;
	mprotect(memory, memory_sz, PROT_READ|PROT_WRITE);

	// Some programs seem to want to play with some high addresses
	mprotect(memory + 0xff000000, 0x01000000, PROT_READ|PROT_WRITE);
}

unsigned int m68k_read_disassembler_8(unsigned int address)
{
	return MEM8(address);
}

unsigned int m68k_read_disassembler_16 (unsigned int address)
{
	return be16toh(MEM16(address));
}

unsigned int m68k_read_disassembler_32 (unsigned int address)
{
	return be64toh(MEM64(address));
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
