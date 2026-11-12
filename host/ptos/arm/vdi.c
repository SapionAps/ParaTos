#include "../host.h"
#include "../vdi.h"

int32_t ptos_native_vdi(paratos_vdi_pb *block)
{
	/* Native ARM GEM trap: selector in r0, native parameter block in r1. */
	register int32_t r0 __asm__("r0") = VDI_TRAP;
	register paratos_vdi_pb *r1 __asm__("r1") = block;
	__asm__ volatile("svc #2" : "+r"(r0), "+r"(r1) :
		: "r2", "r3", "r4", "r5", "r6", "ip", "lr", "cc", "memory");
	return r0;
}
