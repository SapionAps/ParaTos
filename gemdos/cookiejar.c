#include <stdlib.h>
#include "common.h"
#include "gemdos.h"
#include "tos_errors.h"
#include "sysvars.h"

#include "m68k.h"
#include "m68kcpu.h"

uint32_t ResizeCookieJar(int32_t currentSize, int32_t newSize)
{
	uint32_t oldJar = m68k_read_memory_32(_p_cookies);
	uint32_t newJar = Malloc(newSize * 8);
	for(uint32_t slot = 0; slot < currentSize; slot++)
	{
		uint32_t key = m68k_read_memory_32(oldJar+slot*8);
		m68k_write_memory_32(newJar+slot*8, key);
		if (key)
		{
			m68k_write_memory_32(newJar+slot*8+4, m68k_read_memory_32(oldJar+slot*8+4));
		}
		else
		{
			m68k_write_memory_32(newJar+slot*8+4, newSize);
			break;
		}
	}
	m68k_write_memory_32(_p_cookies, newJar);
	Mfree(oldJar);
	// TRACEF("RESIZE %d@%x->%d@%x\n", currentSize, oldJar, newSize, newJar);
	return newJar;
}

void WriteCookie(uint32_t key, uint32_t value)
{
	uint32_t base = m68k_read_memory_32(_p_cookies);
	uint32_t address = base;
	int found = 0;
	int currentCount = 0;
	int maxCount = 0;
	uint32_t current=0;
	for(;;address += 8)
	{
		current = m68k_read_memory_32(address);
		currentCount++;
		if (current == 0)
		{
			maxCount = m68k_read_memory_32(address+4);
			break;
		}

		if (current == key)
		{
			found = 1;
			break;
		}
	}
	if(!found && currentCount == maxCount)
	{
		maxCount *= 2;
		uint32_t newBase = ResizeCookieJar(currentCount, maxCount);
		address += newBase - base;
		base = newBase;
	}
	// TRACEF("Write %0x @ %0x (%0x)\n",key, address, m68k_read_memory_32(address) );
	m68k_write_memory_32(address, key);
	address += 4;
	// TRACEF("Write %0x @ %0x\n",value, address);
	m68k_write_memory_32(address, value);
	address += 4;
	if(!found)
	{
		m68k_write_memory_32(address, 0);
		address += 4;
		m68k_write_memory_32(address, maxCount);
		return;
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


void InitCookieJar()
{
	int numCookies = 0x100;
	uint32_t base = Malloc(numCookies * 8);
	TRACEF("InitCookieJar: %08x\n", base);
	m68k_write_memory_32(_p_cookies, base);
	m68k_write_memory_32(base, 0);
	m68k_write_memory_32(base+4, numCookies);
	WriteCookie(0x4d694e54, 0x200); // MiNT
	WriteCookie(0x5f435055, 20); // _CPU => 20
	WriteCookie(0x5f465055, 0); // _FPU => none
	WriteCookie(0x5f4d4348, 0x60000); // _MCH => 6/0 (paraTos specific)
}
