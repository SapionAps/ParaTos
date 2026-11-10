#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* External callbacks deliberately replace the POSIX inline base+offset
 * accesses: low system memory is emulated, never dereferenced on the host. */
unsigned int m68k_read_memory_8(unsigned int address);
unsigned int m68k_read_memory_16(unsigned int address);
unsigned int m68k_read_memory_32(unsigned int address);
uint64_t m68k_read_memory_64(unsigned int address);
void m68k_write_memory_8(unsigned int address, unsigned int value);
void m68k_write_memory_16(unsigned int address, unsigned int value);
void m68k_write_memory_32(unsigned int address, unsigned int value);
void m68k_write_memory_64(unsigned int address, uint64_t value);

int32_t ptos_memory_init(void);
void ptos_memory_destroy(void);
void ptos_memory_reset_low(void);
void *ptos_guest_buffer(uint32_t address, uint32_t length);
const char *ptos_guest_string(uint32_t address, uint32_t limit);
int32_t ptos_low_read(uint32_t address, unsigned int width, uint32_t *value);
int32_t ptos_low_write(uint32_t address, unsigned int width, uint32_t value);
