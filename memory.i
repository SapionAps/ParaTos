#ifdef PARATOS_PTOS
#include "host/ptos/memory.h"
#else
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include "host/posix/guest_memory.h"
#include "byteorder.h"

extern uint8_t* memory;
#define INLINE __attribute__((always_inline)) static inline 

INLINE uint8_t *guest_pointer(uint32_t address, size_t size)
{
	uint64_t end = (uint64_t)address + size;
	if (end <= POSIX_RAM_BYTES ||
		(UINTPTR_MAX > UINT32_MAX && address >= UINT32_C(0xff000000) &&
		 end <= UINT64_C(0x100000000)))
		return memory + address;
	raise(SIGSEGV);
	abort();
}

// NOTE we currently assume the platform allows unaligned reads and writes.
#define MEM8(address) (*guest_pointer((address), 1))
#define MEM16(address) (*((uint16_t*)guest_pointer((address), 2)))
#define MEM32(address) (*((uint32_t*)guest_pointer((address), 4)))
#define MEM64(address) (*((uint64_t*)guest_pointer((address), 8)))

INLINE unsigned int m68k_read_memory_8(unsigned int address)
{
    return MEM8(address);
}

INLINE unsigned int m68k_read_memory_16(unsigned int address)
{
    return paratos_be16toh(MEM16(address));
}

INLINE unsigned int m68k_read_memory_32(unsigned int address)
{
    return paratos_be32toh(MEM32(address));
}

INLINE uint64_t m68k_read_memory_64(unsigned int address)
{
    return paratos_be64toh(MEM64(address));
}

INLINE void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    MEM8(address) = (uint8_t)value;
}

INLINE void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    MEM16(address) = paratos_htobe16((uint16_t)value);
}

INLINE void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    MEM32(address) = paratos_htobe32((uint32_t)value);
}

INLINE void m68k_write_memory_64(unsigned int address, uint64_t value)
{
	MEM64(address) = paratos_htobe64(value);
}
#endif
