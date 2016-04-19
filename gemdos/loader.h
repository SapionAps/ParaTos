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

typedef struct
{
   uint32_t p_lowtpa;       /* Start address of the TPA            */
   uint32_t p_hitpa;        /* First byte after the end of the TPA */
   uint32_t p_tbase;        /* Start address of the program code   */
   int32_t  p_tlen;         /* Length of the program code          */
   uint32_t p_dbase;        /* Start address of the DATA segment   */
   int32_t  p_dlen;         /* Length of the DATA section          */
   uint32_t p_bbase;        /* Start address of the BSS segment    */
   int32_t  p_blen;         /* Length of the BSS section           */
   uint32_t p_dta;          /* Pointer to the default DTA          */
                            /* Warning: Points first to the        */
                            /* command line !                      */
   uint32_t p_parent;       /* Pointer to the basepage of the      */
                            /* calling processes                   */
   int32_t  p_resrvd0;      /* Reserved                            */
   uint32_t p_env;          /* Address of the environment string   */
   int8_t    p_resrvd1[80]; /* Reserved                            */
   int8_t    p_cmdlin[128]; /* Command line                        */
} __attribute__((packed)) basepage_t;

basepage_t* LoadExe(const char* filename, const char* commandTail);
