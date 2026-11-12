#include "../host.h"
#include "../vdi.h"

#ifdef PARATOS_PTOS_VDI
/* The optional pTOS archive contains a checked userspace client. Its typed
 * VDIPB is marshalled here; generic GEM trap class 2 is never a syscall. */
extern int32_t vdi(paratos_vdi_pb *block);
#endif

int32_t ptos_native_vdi(paratos_vdi_pb *block)
{
#ifdef PARATOS_PTOS_VDI
	return vdi(block);
#else
	(void)block;
	return PTOS_ENOSYS;
#endif
}
