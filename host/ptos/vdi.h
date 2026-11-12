#pragma once
#include "../../vdi/vdi.h"

int32_t ptos_vdi(uint32_t parameter_block);
void ptos_vdi_drop(uint32_t owner);
void ptos_vdi_cleanup(void);
int32_t ptos_native_vdi(paratos_vdi_pb *block);
