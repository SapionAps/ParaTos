#pragma once
#ifdef __APPLE__
#include <machine/endian.h>
#include <libkern/OSByteOrder.h>

#define htobe16(x) OSSwapHostToBigInt16(x)
#define htole16(x) OSSwapHostToLittleInt16(x)
#define be16toh(x) OSSwapBigToHostInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)

#define htobe32(x) OSSwapHostToBigInt32(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)

#define htobe64(x) OSSwapHostToBigInt64(x)
#define htole64(x) OSSwapHostToLittleInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#define le64toh(x) OSSwapLittleToHostInt64(x)

#define __BIG_ENDIAN    BIG_ENDIAN
#define __LITTLE_ENDIAN LITTLE_ENDIAN
#define __BYTE_ORDER    BYTE_ORDER
#endif // __APPLE__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include "m68k.h"
#include "m68kcpu.h"
#include "tos_errors.h"

extern size_t memory_sz;
extern uint8_t* memory;

typedef uint32_t emuptr32_t;
typedef uint32_t emureg_t;

void InitM68KMemory(void);

#ifdef DEBUG
#define TRACEF(...) fprintf(stderr, __VA_ARGS__);
#else
#define TRACEF(...)
#endif

#define NOT_IMPLEMENTED(Subsystem, Name, Op) fprintf(stderr, "Unimplemented " #Subsystem " call " #Name "(%d/%x)\n", Op, Op );
uint64_t m68k_read_memory_64(unsigned int address);
void m68k_write_memory_64(unsigned int address, uint64_t value);
uint32_t m68k_write_string(uint32_t address, const char* value, uint32_t maxLen);
void m68k_read_array8(uint32_t address, char* dest, uint32_t count);
void m68k_write_array8(uint32_t address, const char* value, uint32_t count);

#define m68k_write(address, value) \
	_Generic((value), \
	uint64_t: m68k_write_memory_64((address),(value)), \
	int64_t: m68k_write_memory_64((address),(value)), \
	uint32_t: m68k_write_memory_32((address),(value)), \
	int32_t: m68k_write_memory_32((address),(value)), \
	uint16_t: m68k_write_memory_16((address),(value)), \
	int16_t: m68k_write_memory_16((address),(value)), \
	uint8_t: m68k_write_memory_8((address),(value)), \
	int8_t: m68k_write_memory_8((address),(value)), \
	int8_t[sizeof(value)] : m68k_write_array8((address),(char*)(intptr_t)(value),sizeof(value)), \
	int8_t* : m68k_write_string((address),(char*)(intptr_t)(value),1024) \
)

#define m68k_read(address, typed_dummy_value) \
	_Generic((typed_dummy_value), \
	uint64_t: m68k_read_memory_64((address)), \
	int64_t: m68k_read_memory_64((address)), \
	uint32_t: m68k_read_memory_32((address)), \
	int32_t: m68k_read_memory_32((address)), \
	uint16_t: m68k_read_memory_16((address)), \
	int16_t: m68k_read_memory_16((address)), \
	uint8_t: m68k_read_memory_8((address)), \
	int8_t: m68k_read_memory_8((address)) \
)

void m68k_read_to_64(uint32_t address, uint64_t* dest);
void m68k_read_to_32(uint32_t address, uint32_t* dest);
void m68k_read_to_16(uint32_t address, uint16_t* dest);
void m68k_read_to_8(uint32_t address, uint8_t* dest);

#define m68k_read_to(address, lval) ({\
	_Generic((lval), \
	uint64_t: m68k_read_to_64((address),(uint64_t*)&(lval)), \
	int64_t: m68k_read_to_64((address),(uint64_t*)&(lval)), \
	uint32_t: m68k_read_to_32((address),(uint32_t*)&(lval)), \
	int32_t: m68k_read_to_32((address),(uint32_t*)&(lval)), \
	uint16_t: m68k_read_to_16((address),(uint16_t*)&(lval)), \
	int16_t: m68k_read_to_16((address),(uint16_t*)&(lval)), \
	uint8_t: m68k_read_to_8((address),(uint8_t*)&(lval)), \
	int8_t: m68k_read_to_8((address),(uint8_t*)&(lval)), \
	int8_t[sizeof(lval)] : m68k_read_array8((address),(char*)(intptr_t)(lval),sizeof(lval)), \
);})

#define m68k_write_field(address, type, field, value) do { \
	m68k_write((address) + offsetof(type, field), (__typeof__(((type *)0)->field))(value)); \
} while (0)

#define m68k_write_struct_member(address, value, field) \
	m68k_write((address) + offsetof(__typeof__(value), field), (value).field)

#define m68k_read_field(address, type, field) \
	m68k_read((address) + offsetof(type, field), (((type *)0)->field))

#define m68k_read_field_to(address, type, field, dest) \
	m68k_read_to((address) + offsetof(type, field), dest )

#define m68k_read_struct_member(address, value, field) \
	m68k_read_field_to((address), __typeof__(value), field, (value).field)
