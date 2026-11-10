#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ops.h"

/* Shared native-call API; native.c delegates to arm/ or x32/. Core tests
 * replace this boundary without issuing host traps. */
int32_t ptos_call(uint16_t function, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
int32_t ptos_map(uint32_t address, uint32_t length);
int32_t ptos_unmap(uint32_t address, uint32_t length);

#define PTOS_NATIVE_BASE 0x41000000u
#define PTOS_RAM_START   0x00010000u
#define PTOS_RAM_END     0x01000000u
#define PTOS_PAGE_BYTES  4096u

#define PTOS_ENOSYS (-32)
#define PTOS_EINVAL (-25)
#define PTOS_ENSMEM (-39)
#define PTOS_EIMBA  (-40)
#define PTOS_ERANGE (-64)
#define PTOS_EPLFMT (-66)

_Noreturn void ptos_fault(uint32_t address, const char *reason);
