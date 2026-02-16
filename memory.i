#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include "byteorder.h"

extern uint8_t* memory;
#define INLINE __attribute__((always_inline)) static inline 

// NOTE we currently assume the platform allows unaligned reads and writes.
#define MEM8(address) (memory[(address)])
#define MEM16(address) (*((uint16_t*)(memory+(address))))
#define MEM32(address) (*((uint32_t*)(memory+(address))))
#define MEM64(address) (*((uint64_t*)(memory+(address))))

INLINE unsigned int m68k_read_memory_8(unsigned int address)
{
    return MEM8(address);
}

INLINE unsigned int m68k_read_memory_16(unsigned int address)
{
    return be16toh(MEM16(address));
}

INLINE unsigned int m68k_read_memory_32(unsigned int address)
{
    return be32toh(MEM32(address));
}

INLINE uint64_t m68k_read_memory_64(unsigned int address)
{
    return be64toh(MEM64(address));
}

INLINE void m68k_write_memory_8(unsigned int address, unsigned int value)
{
    MEM8(address) = (uint8_t)value;
}

INLINE void m68k_write_memory_16(unsigned int address, unsigned int value)
{
    MEM16(address) = htobe16((uint16_t)value);
}

INLINE void m68k_write_memory_32(unsigned int address, unsigned int value)
{
    MEM32(address) = htobe32((uint32_t)value);
}

INLINE void m68k_write_memory_64(unsigned int address, uint64_t value)
{
	MEM64(address) = htobe64(value);
}
