#pragma once
#include <stdint.h>
typedef struct  {
	uint16_t PRG_magic;
	int32_t PRG_tsize;
	int32_t PRG_dsize;
	int32_t PRG_bsize;
	int32_t PRG_ssize;
	int32_t PRG_res1;
	uint32_t PRGFLAGS;
	uint16_t ABSFLAG;
} __attribute__((packed)) tos_header_t;

basepage_t* LoadExe(const char* filename, char* argv[], int argc);
