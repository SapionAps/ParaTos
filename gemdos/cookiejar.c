#include <stdlib.h>
#include "common.h"
#include "tos_errors.h"
#include "sysvars.h"

#include "m68k.h"
#include "m68kcpu.h"

void WriteCookie(uint32_t key, uint32_t value)
{
	uint32_t address = m68k_read_memory_32(_p_cookies);
	int found = 0;
	uint32_t current=0;
	for(;;address += 8)
	{
		current = m68k_read_memory_32(address);
		if (current == 0)
		{
			break;
		}

		if (current == key)
		{
			found = 1;
			break;
		}
	}
	printf("Write %0x @ %0x (%0x)\n",key, address, m68k_read_memory_32(address) );
	m68k_write_memory_32(address, key);
	address += 4;
	printf("Write %0x @ %0x\n",value, address);
	m68k_write_memory_32(address, value);
	address += 4;
	if(!found)
	{
		m68k_write_memory_32(address, 0);
		address += 4;
		m68k_write_memory_32(address, 0);
	}
}

int ReadCookie(uint32_t key, uint32_t* value)
{
	uint32_t address = m68k_read_memory_32(_p_cookies);
	int found = 0;
	if (key > 1 && key <= 0xffff)
	{
		for(int32_t i=1; i <= key; address += 8, i++)
		{
			if (m68k_read_memory_32(address) == 0)
			{
				return 0;
			}
		}
		*value=m68k_read_memory_32(address);
		return 1;
	}
	else
	{
		uint32_t current=0;
		for(;;address += 8)
		{
			current = m68k_read_memory_32(address);
			if (current == 0)
			{
				break;
			}

			if (current == key)
			{
				found = 1;
				break;
			}
		}
	}

	if (found)
		*value=m68k_read_memory_32(address+4);
	return found-1;
}


void InitCookieJar(uint32_t base)
{
	printf("InitCookieJar: %08x\n", base);
	m68k_write_memory_32(_p_cookies, base);
	m68k_write_memory_32(base, 0);
	WriteCookie(0x4d694e54, 0x110); // MiNT
	WriteCookie(0x5f435055, 20); // _CPU => 20
	WriteCookie(0x5f465055, 0); // _FPU => none
}
