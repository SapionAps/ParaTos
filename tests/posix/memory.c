#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "common.h"
#include "gemdos/gemdos.h"
#include "host.h"

int main(void)
{
	static const uint8_t expected[] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0};
	InitM68KMemory();
	assert(memory_sz == 16 * 1024 * 1024);
	m68k_write_memory_64(0x100, UINT64_C(0x123456789abcdef0));
	assert(!memcmp(memory + 0x100, expected, sizeof(expected)));
	assert(m68k_read_memory_64(0x100) == UINT64_C(0x123456789abcdef0));
	assert(m68k_read_memory_32(0x100) == UINT32_C(0x12345678));
	assert(m68k_read_memory_16(0x104) == 0x9abc);
	assert(m68k_read_memory_8(0x107) == 0xf0);
	m68k_write_memory_32(0x100, UINT32_C(0x89abcdef));
	m68k_write_memory_16(0x104, 0x4321);
	m68k_write_memory_8(0x106, 0x55);
	assert(memory[0x100] == 0x89 && memory[0x103] == 0xef);
	assert(memory[0x104] == 0x43 && memory[0x105] == 0x21 && memory[0x106] == 0x55);
	char copy[sizeof(expected)];
	m68k_write_array8(0x200, (const char*)expected, sizeof(expected));
	m68k_read_array8(0x200, copy, sizeof(copy));
	assert(!memcmp(copy, expected, sizeof(copy)));
	current_process = 0x300;
	Fsetdta(0x500);
	assert(Fgetdta() == 0x500);
	assert(Pdomain(-1) == 0);
	assert(Pdomain(1) == 1 && Pdomain(-1) == 1);
	assert(Pdomain(2) == -1 && Pdomain(-1) == 1);
	return 0;
}
