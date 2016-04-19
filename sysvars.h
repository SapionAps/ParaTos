/* definitions for various GEMDOS system variables in low memory */
/* WARNING: this file is not compatible with the old one */

/*
 * Convention:
 * 	the address for each variable is defined.
 */
#pragma once

/* timer, crit error and process termination handoff vectors */
#define etv_timer	((void (**)()) 0x400L)
#define etv_critic	((void (**)()) 0x404L)
#define etv_term	((void (**)()) 0x408L)

/* memory controller */
#define MEMVALID_MAGIC	0x752019F3L	/* once memory is sized */
#define memvalid	(/* (unsigned long *) */ 0x420L)
#define mencntlr	(/* /* (unsigned char *) */ */ 0x424L)
 			/* 0 = 128K, 4 = 512K 0 = 256k(2banks) 5 = 1M */

/* reset vector, jump through resvector if resvalid on reset */
#define RESVALID_MAGIC	0x31415926L
#define resvalid	(/* (unsigned long *) */ 0x426L)
#define resvector	((void (**)())     0x42aL)
			/* do a jmp 0x24L(a6) at end to go to system reset */

/* mem */
#define phystop		(/* (unsigned long *) */ 0x42eL) /* physical top of st ram */
#define _membot		(/* (unsigned long *) */ 0x432L) /* bottom of avail     */
#define _memtop		(/* (unsigned long *) */ 0x436L) /* top    of avail     */
#define MEMVAL2_MAGIC	0x237698AAL /* after suc. coldstart && memvalid     */
#define memval2		(/* (unsigned long *) */ 0x43aL)
#define ramtop		(/* (unsigned long *) */ 0x5a4L) /* physical top of tt ram */

/* floppy */
#define flock		(/* (short *) */ 0x43eL) /* lock usage of DMA   chip */
#define seekrate	(/* (short *) */ 0x440L) /* 0=6ms 1=12ms 2=2ms 3=3ms */
#define _timr_ms	(/* (short *) */ 0x442L) /* timer calib == 20ms      */
#define _fverify	(/* (short *) */ 0x444L) /* write verify flag        */
#define _bootdev	(/* (short *) */ 0x446L)

/* video */
#define palmode		(/* (short *) */ 0x448L) /* PAL video mode flag         */
#define defshiftmd	(/* (unsigned char *) */ 0x44aL) /* default video rez   */
#define sshiftmd	(/* (short *) */ 0x44cL) /* shadow of hdwr. shiftmd reg */
 					   /* 0=Lo 1=med 2=Hi rez         */
#define _v_bas_ad	(/* (void *) */ 0x44eL)  /* screen mem base             */
#define vblsem		(/* (short *) */ 0x452L) /* vbl semaphore               */
#define nvbls		(/* (short *) */ 0x454L) /* # of vbl entries def. == 8  */
#define _vblqueue	((void (***)()) 0x456L) /* vbl queue pointer      */
#define colorptr	((short **) 0x45aL) /* pal. on next vblank if!NULL */
#define _vbclock	(/* (unsigned long *) */ 0x462L) /* vbi counter         */
#define _frclock        (/* (unsigned long *) */ 0x466L) /* #vbi not vblsem'ed  */

#define _hz_200		(/* (unsigned long *) */ 0x4baL)

#define conterm		(*/* (char *) */ 0x484L)
#define savptr		(/* (long *) */ 0x4A2L)
#define _nflops		(/* (short *) */ 0x4A6L)
#define _sysbase	(/* (long *) */ 0x4F2L)
#define _shell_p	(/* (long *) */ 0x4F6L)

typedef struct {
  short	puns;
  char	v_p_un[16];
  long	pstart[16];
  short	bpbs[1];		/* really 16 BPB's (bios parameter block) */
} HDINFO;

#define pun_ptr		(/* (HDINFO *) */ 0x516L)
#define _p_cookies	(/* (long **) */ 0x5A0L)

/* os header */
typedef struct _osheader
{
    unsigned short	os_entry;   	 /* 0x00  BRA to reset handler	*/
    unsigned short	os_version; 	 /* 0x02  TOS version		*/
    void		(*reseth) (void); /* 0x04 -> reset handler	*/
    struct _osheader	*os_beg;	 /* 0x08 -> base of OS		*/
    void		*os_end;	 /* 0x0c -> end of OS ram usage */
    char		*os_rsv1;	 /* 0x10 reserved		*/
    char		*os_magic;	 /* 0x14 GEM memory usage param */
    long		os_date;	 /* 0x18 Build date 0xMMDDYYYY	*/
    unsigned short	os_conf;	 /* 0x1c OS conf bits		*/
    unsigned short	os_dosdate;	 /* 0x1e DOS format build date  */
    /* the following available on TOS version >= 1.2 */
    char		**p_root;	 /* 0x20 -> base of OS pool	*/
    char		**pkbshift;	 /* 0x24 -> kbd shift state var */
    char		**p_run;	 /* 0x28 -> PID of current proc */
    char		*p_rsv2;	 /* 0x2c reserved		*/
} OSHEADER;
