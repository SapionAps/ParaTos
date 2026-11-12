#include "../host.h"
#include "../vdi.h"

int32_t ptos_native_vdi(paratos_vdi_pb *block)
{
	/* pTOS ARM has no ring-3 typed VDI client yet. Do not invoke its trusted
 * kernel VDIPB entry with userspace pointers or execute 68k callbacks. */
	(void)block;
	return PTOS_ENOSYS;
}
