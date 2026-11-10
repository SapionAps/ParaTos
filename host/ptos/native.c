#include "host.h"

/* The first backend is pTOS/x32 (ILP32 long-mode userspace). These are pTOS
 * trap numbers, not Linux syscall numbers. Never compile this for POSIX. */
#if !defined(__x86_64__) || !defined(__ILP32__)
#error "The initial pTOS backend requires the x32 ABI (-mx32)"
#endif

static int32_t call6(uint16_t fn, intptr_t a, intptr_t b, intptr_t c,
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

int32_t ptos_call(uint16_t fn, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
	return call6(fn, a, b, c, d, 0, 0);
}

int32_t ptos_map(uint32_t address, uint32_t length)
{
	/* PRIVATE | ANONYMOUS | FIXED_NOREPLACE, RW (guest code is CPU data). */
	return call6(0x17e, address, length, 3, 0x100022, -1, 0);
}

int32_t ptos_unmap(uint32_t address, uint32_t length)
{
	return ptos_call(0x17f, address, length, 0, 0);
}
