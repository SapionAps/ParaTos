#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include "m68k.h"
#include "m68kcpu.h"
#include "tos_errors.h"

extern uint32_t memory_sz;
extern uint8_t* memory;

typedef uint32_t emuptr32_t;
typedef uint32_t emureg_t;

#define NOT_IMPLEMENTED(Subsystem, Name, Op) fprintf(stderr, "Unimplemented " #Subsystem " call " #Name "(%d/%x)\n", Op, Op );
uint64_t m68k_read_memory_64(unsigned int address);
void m68k_write_memory_64(unsigned int address, uint64_t value);
uint32_t m68k_read_string(uint32_t address, char* dest, uint32_t maxLen, int is_path);
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
	int8_t* : m68k_read_string((address),(char*)(intptr_t)(lval),1024,0) \
);})

#define m68k_write_field(address, type, field, value) ({ \
	m68k_write((address) + offsetof(type, field), (typeof(((type *)0)->field))(value)); \
})

#define m68k_write_struct_member(address, value, field) \
	m68k_write((address) + offsetof(typeof(value), field), (value).field)

#define m68k_read_field(address, type, field) \
	m68k_read((address) + offsetof(type, field), (((type *)0)->field))

#define m68k_read_field_to(address, type, field, dest) \
	m68k_read_to((address) + offsetof(type, field), dest )

#define m68k_read_struct_member(address, value, field) \
	m68k_read_field_to((address), typeof(value), field, (value).field)
