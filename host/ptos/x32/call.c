#include "../native.h"

#if !defined(__x86_64__) || !defined(__ILP32__)
#error "The pTOS x32 syscall bridge requires the x32 ABI (-mx32)"
#endif

int32_t ptos_native_call6(uint16_t fn, intptr_t a, intptr_t b, intptr_t c,
	intptr_t d, intptr_t e, intptr_t f)
{
	register uint64_t ax __asm__("rax") = ((uint64_t)1 << 32) | fn;
	register int64_t di __asm__("rdi") = a;
	register int64_t si __asm__("rsi") = b;
	register int64_t dx __asm__("rdx") = c;
	register int64_t r10 __asm__("r10") = d;
	register int64_t r8 __asm__("r8") = e;
	register int64_t r9 __asm__("r9") = f;
	__asm__ volatile("syscall" : "+a"(ax), "+r"(di), "+r"(si), "+r"(dx),
		"+r"(r10), "+r"(r8), "+r"(r9) : : "rcx", "r11", "cc", "memory");
	return (int32_t)ax;
}

int32_t ptos_native_bios_call(uint16_t fn, int16_t device, int16_t value)
{
	register uint64_t ax __asm__("rax") = ((uint64_t)13 << 32) | fn;
	register int64_t di __asm__("rdi") = device;
	register int64_t si __asm__("rsi") = value;
	__asm__ volatile("syscall" : "+a"(ax), "+r"(di), "+r"(si) :
		: "rdx", "r10", "r8", "r9", "rcx", "r11", "cc", "memory");
	return (int32_t)ax;
}
