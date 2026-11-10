#include "../native.h"

#if !defined(__arm__)
#error "The pTOS ARM syscall bridge requires an ARM toolchain"
#endif

int32_t ptos_native_call6(uint16_t fn, intptr_t a, intptr_t b, intptr_t c,
	intptr_t d, intptr_t e, intptr_t f)
{
	register int32_t r0 __asm__("r0") = fn;
	register int32_t r1 __asm__("r1") = a;
	register int32_t r2 __asm__("r2") = b;
	register int32_t r3 __asm__("r3") = c;
	register int32_t r4 __asm__("r4") = d;
	register int32_t r5 __asm__("r5") = e;
	register int32_t r6 __asm__("r6") = f;
	__asm__ volatile("svc #1" : "+r"(r0), "+r"(r1), "+r"(r2), "+r"(r3),
		"+r"(r4), "+r"(r5), "+r"(r6) : : "ip", "lr", "cc", "memory");
	return r0;
}

int32_t ptos_native_bios_call(uint16_t fn, int16_t device, int16_t value)
{
	register int32_t r0 __asm__("r0") = fn;
	register int32_t r1 __asm__("r1") = device;
	register int32_t r2 __asm__("r2") = value;
	__asm__ volatile("svc #13" : "+r"(r0), "+r"(r1), "+r"(r2) :
		: "r3", "r4", "r5", "r6", "ip", "lr", "cc", "memory");
	return r0;
}
