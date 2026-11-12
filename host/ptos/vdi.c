#include <stdlib.h>
#include "host.h"
#include "guest.h"
#include "vdi.h"

typedef struct workstation {
	struct workstation *next;
	uint32_t owner;
	int16_t handle;
} workstation;
static workstation *workstations;

static int guest_range(uint32_t address, uint32_t bytes, int writing)
{
	(void)writing; /* Mapped guest RAM is private read/write CPU data. */
	return address >= PTOS_RAM_START && address < PTOS_RAM_END && bytes <= PTOS_RAM_END - address;
}

static int32_t close_workstation(int16_t handle)
{
	paratos_vdi_control control = {.code = VDI_CLOSE, .handle = handle};
	paratos_vdi_pb block = {.control = &control};
	int32_t result = ptos_native_vdi(&block);
	return result == PTOS_EIHNDL ? 0 : result; /* Already closed natively. */
}

static int32_t invoke(paratos_vdi_pb *block)
{
	paratos_vdi_control *c = block->control;
	ptos_vdi_drop(0); /* Retry failed cleanup without touching live guests. */
	if (c->code == VDI_OPEN)
	{
		workstation *p = calloc(1, sizeof(*p));
		if (!p) return PTOS_ENSMEM;
		int32_t result = ptos_native_vdi(block);
		if (result < 0) { free(p); return result; }
		p->handle = c->handle;
		p->owner = ptos_process;
		p->next = workstations; workstations = p;
		if (c->handle <= 0 || c->nintout < 0 || c->nintout > 45 || c->nptsout < 0 || c->nptsout > 6)
		{
			p->owner = 0;
			ptos_vdi_drop(0);
			return PTOS_EINVAL;
		}
		return result;
	}
	for (workstation **at = &workstations; *at; at = &(*at)->next)
	{
		workstation *p = *at;
		if (p->owner != ptos_process || p->handle != c->handle) continue;
		int closing = c->code == VDI_CLOSE;
		int32_t result = ptos_native_vdi(block);
		if ((result >= 0 && closing) || result == PTOS_EIHNDL)
		{
			*at = p->next; free(p);
		}
		return result;
	}
	return PTOS_EIHNDL;
}

int32_t ptos_vdi(uint32_t parameter_block)
{
	static const paratos_vdi_host host = {guest_range, invoke};
	return paratos_vdi_marshal(parameter_block, &host);
}

void ptos_vdi_drop(uint32_t owner)
{
	for (workstation **at = &workstations; *at; )
	{
		workstation *p = *at;
		if (p->owner != owner) { at = &p->next; continue; }
		if (p->handle > 0 && close_workstation(p->handle) < 0)
		{
			p->owner = 0; at = &p->next; continue;
		}
		*at = p->next; free(p);
	}
}

void ptos_vdi_cleanup(void)
{
	for (workstation *p = workstations; p; p = p->next) p->owner = 0;
	ptos_vdi_drop(0);
}
