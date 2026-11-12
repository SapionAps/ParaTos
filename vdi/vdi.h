#pragma once
#include <stdint.h>

/* Native pTOS userspace VDI ABI. These are never kernel parameter blocks.
 * Native pointers are 32 bits on pTOS; host tests use their own pointer size. */
typedef struct paratos_vdi_fdb {
	void *address;
	int16_t width, height, stride, standard, planes, reserved[3];
} paratos_vdi_fdb;

typedef struct paratos_vdi_control {
	int16_t code, nptsin, nptsout, nintin, nintout, subcode, handle, reserved;
	union {
		uint32_t reserved[3];
		struct { paratos_vdi_fdb *source, *destination; } raster;
	} args;
} paratos_vdi_control;

typedef struct paratos_vdi_pb {
	paratos_vdi_control *control;
	int16_t *intin, *ptsin, *intout, *ptsout;
} paratos_vdi_pb;

typedef struct paratos_vdi_host {
	int (*range)(uint32_t address, uint32_t bytes, int writing);
	int32_t (*call)(paratos_vdi_pb *block);
} paratos_vdi_host;

enum paratos_vdi_operation {
	VDI_POLYLINE = 6, VDI_TEXT = 8, VDI_GDP = 11, VDI_TEXT_HEIGHT = 12,
	VDI_LINE_TYPE = 15, VDI_LINE_WIDTH = 16, VDI_LINE_COLOR = 17,
	VDI_TEXT_COLOR = 22, VDI_FILL_INTERIOR = 23, VDI_FILL_STYLE = 24,
	VDI_FILL_COLOR = 25, VDI_WRITE_MODE = 32, VDI_TEXT_ATTRIBUTES = 38,
	VDI_OPEN = 100, VDI_CLOSE = 101, VDI_INQUIRE = 102, VDI_PIXEL = 105,
	VDI_COPY = 109, VDI_TRANSFORM = 110, VDI_LINE_PATTERN = 113,
	VDI_RECTANGLE = 114, VDI_TRANSPARENT = 121, VDI_CLIP = 129,
	VDI_TRAP = 0x73
};

#define VDI_MAX_INTEGERS 256
#define VDI_MAX_POINTS 256
#define VDI_MAX_BITMAP (1024u * 1024u)
#define VDI_ENOSYS (-32)
#define VDI_EINVAL (-25)
#define VDI_ENSMEM (-39)
#define VDI_EIMBA (-40)

int32_t paratos_vdi_marshal(uint32_t block, const paratos_vdi_host *host);
