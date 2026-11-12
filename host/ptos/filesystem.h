#pragma once
#include <stdint.h>

/* pTOS ARM/x32 native layouts. Guest offsets are explicitly marshalled. */
typedef struct ptos_native_dta {
	uint8_t reserved[21], attribute;
	uint16_t time, date;
	uint32_t size;
	char name[14];
} ptos_native_dta;

typedef struct ptos_native_xattr {
	uint16_t mode;
	uint32_t index;
	uint16_t dev, rdev, nlink, uid, gid;
	int32_t size, blksize, blocks;
	struct { uint16_t time, date; } stamp[3];
	uint16_t attribute, reserved1;
	uint32_t reserved2[2];
} ptos_native_xattr;

int32_t ptos_search(uint16_t function, const char *path, uint16_t attribute);
int32_t ptos_disk_free(uint32_t buffer, uint16_t drive);
int32_t ptos_xattr(uint16_t flag, const char *path, uint32_t buffer);
void ptos_filesystem_drop(uint32_t owner);
void ptos_filesystem_cleanup(void);
