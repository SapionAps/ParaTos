#pragma once

#include <stdint.h>
#include <stdio.h>
#include "tos_errors.h"
extern uint32_t memory_sz;
extern uint8_t* memory;

typedef uint32_t emuptr32_t;
typedef uint32_t emureg_t;

#define NOT_IMPLEMENTED(Subsystem, Name, Op) fprintf(stderr, "Unimplemented " #Subsystem " call " #Name "(%d/%x)\n", Op, Op );
uint64_t m68k_read_memory_64(unsigned int address);
void m68k_write_memory_64(unsigned int address, uint64_t value);
uint32_t m68k_read_string(uint32_t address, char* dest, uint32_t maxLen, int is_path);
