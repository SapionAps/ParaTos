#include <string.h>
#include "host.h"
#include "memory.h"

static uint8_t low[PTOS_RAM_START];
static int mapped;

/* Only scalar system variables with a guest-compatible meaning are live.
 * Pointer-bearing fields and vectors stay in the guest-local shadow. */
static const struct { uint16_t address; uint8_t width; } live[] = {
	{0x43e, 2}, {0x440, 2}, {0x442, 2}, {0x444, 2}, {0x446, 2},
	{0x462, 4}, {0x466, 4}, {0x484, 1}, {0x4a6, 2}, {0x4ba, 4}
};

void ptos_memory_reset_low(void)
{
	memset(low, 0, sizeof(low));
	/* Guest memory limits and CPU identity must not describe the host. */
	m68k_write_memory_32(0x42e, PTOS_RAM_END);
	m68k_write_memory_32(0x432, PTOS_RAM_START);
	m68k_write_memory_32(0x436, PTOS_RAM_END);
	m68k_write_memory_32(0x5a0, 0x1000);
	m68k_write_memory_32(0x1000, 0x5f435055); /* _CPU = 68020 */
	m68k_write_memory_32(0x1004, 20);
	m68k_write_memory_32(0x1008, 0x5f465055); /* _FPU = none */
	m68k_write_memory_32(0x1010, 0);
	m68k_write_memory_32(0x1014, 3);
}

int32_t ptos_memory_init(void)
{
	if (mapped)
		return PTOS_EINVAL;
	int32_t result = ptos_map(PTOS_RAM_START, PTOS_RAM_END - PTOS_RAM_START);
	if (result < 0)
		return result;
	if ((uint32_t)result != PTOS_RAM_START)
	{
		ptos_unmap((uint32_t)result, PTOS_RAM_END - PTOS_RAM_START);
		return PTOS_EIMBA;
	}
	mapped = 1;
	ptos_memory_reset_low();
	return 0;
}

void ptos_memory_destroy(void)
{
	if (mapped)
		ptos_unmap(PTOS_RAM_START, PTOS_RAM_END - PTOS_RAM_START);
	mapped = 0;
}

void *ptos_guest_buffer(uint32_t address, uint32_t length)
{
	if (!length) return (void *)(uintptr_t)address;
	if (address < PTOS_RAM_START || address >= PTOS_RAM_END || length > PTOS_RAM_END - address)
		ptos_fault(address, "buffer outside guest RAM");
	return (void *)(uintptr_t)address;
}

const char *ptos_guest_string(uint32_t address, uint32_t limit)
{
	if (!limit)
		ptos_fault(address, "empty string range");
	uint32_t count = address < PTOS_RAM_END ? PTOS_RAM_END - address : 0;
	if (count > limit)
		count = limit;
	const char *s = ptos_guest_buffer(address, count);
	if (!memchr(s, 0, count))
		ptos_fault(address, "unterminated guest string");
	return s;
}

static int width_mode(unsigned int width, int writing)
{
	return (writing ? 0x0d : 0x0a) + (width == 4 ? 0 : width == 2 ? 1 : 2);
}

int32_t ptos_low_read(uint32_t address, unsigned int width, uint32_t *value)
{
	if ((width != 1 && width != 2 && width != 4) || address >= sizeof(low) || width > sizeof(low) - address)
		return PTOS_EINVAL;
	/* Refresh whole host fields, then slice in guest big-endian order. This
	 * handles byte/word accesses and reads crossing two adjacent fields. */
	for (size_t i = 0; i < sizeof(live) / sizeof(live[0]); i++)
	{
		uint32_t base = live[i].address, n = live[i].width;
		if (address >= base + n || base >= address + width)
			continue;
		int32_t r = ptos_call(0x154, width_mode(n, 0), base, 0, 0);
		if (r == PTOS_ENOSYS || r == -36)
			return r;
		for (uint32_t j = 0; j < n; j++)
			low[base + j] = (uint32_t)r >> ((n - j - 1) * 8);
	}
	*value = 0;
	for (unsigned int i = 0; i < width; i++)
		*value = (*value << 8) | low[address + i];
	return 0;
}

int32_t ptos_low_write(uint32_t address, unsigned int width, uint32_t value)
{
	if ((width != 1 && width != 2 && width != 4) || address >= sizeof(low) || width > sizeof(low) - address)
		return PTOS_EINVAL;
	/* Refuse cross-field or partial host writes rather than modifying half
	 * a system variable or bypassing Ssystem's access restrictions. */
	for (size_t i = 0; i < sizeof(live) / sizeof(live[0]); i++)
	{
		uint32_t base = live[i].address, n = live[i].width;
		if (address >= base + n || base >= address + width)
			continue;
		if (address != base || width != n)
			return PTOS_EINVAL;
		int32_t r = ptos_call(0x154, width_mode(n, 1), base, value, 0);
		if (r < 0)
			return r;
	}
	for (unsigned int i = 0; i < width; i++)
		low[address + i] = value >> ((width - i - 1) * 8);
	return 0;
}

static uint32_t read_value(uint32_t address, unsigned int width)
{
	uint32_t result = 0;
	if (address < sizeof(low) && width <= sizeof(low) - address)
	{
		if (ptos_low_read(address, width, &result) < 0)
			ptos_fault(address, "unsupported low-memory read");
		return result;
	}
	/* Byte assembly supports unaligned accesses on ARM as well as x32. */
	if (address > UINT32_MAX - (width - 1))
		ptos_fault(address, "wrapping guest read");
	for (unsigned int i = 0; i < width; i++)
	{
		uint32_t a = address + i;
		uint32_t byte;
		if (a < sizeof(low))
		{
			if (ptos_low_read(a, 1, &byte) < 0)
				ptos_fault(a, "unsupported low-memory read");
		}
		else
			byte = *(uint8_t *)ptos_guest_buffer(a, 1);
		result = (result << 8) | byte;
	}
	return result;
}

static void write_value(uint32_t address, unsigned int width, uint32_t value)
{
	if (address < sizeof(low))
	{
		if (width > sizeof(low) - address || ptos_low_write(address, width, value) < 0)
			ptos_fault(address, "unsupported low-memory write");
		return;
	}
	uint8_t *p = ptos_guest_buffer(address, width);
	for (unsigned int i = 0; i < width; i++)
		p[i] = value >> ((width - i - 1) * 8);
}

unsigned int m68k_read_memory_8(unsigned int a) { return read_value(a, 1); }
unsigned int m68k_read_memory_16(unsigned int a) { return read_value(a, 2); }
unsigned int m68k_read_memory_32(unsigned int a) { return read_value(a, 4); }
uint64_t m68k_read_memory_64(unsigned int a)
{
	if (a > UINT32_MAX - 7) ptos_fault(a, "wrapping guest read");
	return ((uint64_t)read_value(a, 4) << 32) | read_value(a + 4, 4);
}
void m68k_write_memory_8(unsigned int a, unsigned int v) { write_value(a, 1, v); }
void m68k_write_memory_16(unsigned int a, unsigned int v) { write_value(a, 2, v); }
void m68k_write_memory_32(unsigned int a, unsigned int v) { write_value(a, 4, v); }
void m68k_write_memory_64(unsigned int a, uint64_t v)
{
	if (a > UINT32_MAX - 7) ptos_fault(a, "wrapping guest write");
	write_value(a, 4, v >> 32); write_value(a + 4, 4, v);
}
unsigned int m68k_read_disassembler_8(unsigned int a) { return read_value(a, 1); }
unsigned int m68k_read_disassembler_16(unsigned int a) { return read_value(a, 2); }
unsigned int m68k_read_disassembler_32(unsigned int a) { return read_value(a, 4); }
