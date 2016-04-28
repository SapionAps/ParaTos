#pragma once

// Character input/output

int32_t Cauxin ( void );
int16_t Cauxis ( void );
int16_t Cauxos ( void );
int32_t Cauxout ( int16_t c );
int32_t Cconin ( void );
int32_t Cconis ( void );
int16_t Cconos ( void );
int32_t Cconout ( int16_t c );
int32_t Cconrs ( emuptr32_t buf );
int32_t Cconws ( emuptr32_t buf );
int32_t Cnecin ( void );
int16_t Cprnos ( void );
int32_t Cprnout ( int16_t c );
int32_t Crawcin ( void );
int32_t Crawio ( int16_t w );

// Date and Time

int32_t Talarm ( int32_t time );
uint32_t Tgetdate ( void );
uint32_t Tgettime ( void );
int32_t Tgettimeofday ( emuptr32_t tv, emuptr32_t tzp );
int32_t Tmalarm( int32_t time );
int16_t Tsetdate ( uint16_t date );
int32_t Tsetitimer ( int16_t which, emuptr32_t interval, emuptr32_t value, emuptr32_t ointerval, emuptr32_t ovalue );
int16_t Tsettime ( uint16_t time );
int32_t Tsettimeofday ( emuptr32_t tv, emuptr32_t tzp );

// Directory functions

int32_t Dchroot( emuptr32_t path );
int32_t Dclosedir ( int32_t dirhandle );
int32_t Dcntl ( int16_t cmd, emuptr32_t name, int32_t arg );
int32_t Dcreate ( emuptr32_t path );
int32_t Ddelete ( emuptr32_t path );
int16_t Dfree ( emuptr32_t buf, int16_t driveno );
int32_t Dgetcwd ( emuptr32_t path, int16_t drv, int16_t size );
int16_t Dgetdrv ( void );
int16_t Dgetpath ( emuptr32_t path, int16_t driveno );
int32_t Dlock ( int16_t mode, int16_t drv );
int32_t Dopendir ( emuptr32_t name, int16_t flag );
int32_t Dpathconf ( emuptr32_t name, int16_t mode );
int32_t Dreaddir ( int16_t len, int32_t dirhandle, emuptr32_t buf );
int32_t Dreadlabel ( emuptr32_t path, emuptr32_t label, int16_t length );
int32_t Drewinddir ( int32_t handle );
int32_t Dsetdrv ( int16_t drv );
int16_t Dsetpath ( emuptr32_t path );
int32_t Dwritelabel ( emuptr32_t path, emuptr32_t label );
int32_t Dxopendir ( emuptr32_t name, int16_t flag );
int32_t Dxreaddir ( int16_t ln, int32_t dirh, emuptr32_t buf, emuptr32_t xattr, emuptr32_t xr );

// File functions

int16_t Fattrib ( emuptr32_t filename, int16_t wflag, int16_t attrib );
int32_t Fchmod ( emuptr32_t name, int16_t mode );
int32_t Fchown ( emuptr32_t name, int16_t uid, int16_t gid );
int16_t Fclose ( int16_t handle );
int32_t Fcntl ( int16_t fh, int32_t arg, int16_t cmd );
int16_t Fcreate ( emuptr32_t fname, int16_t attr );
void Fdatime ( emuptr32_t timeptr, int16_t handle, int16_t wflag );
int16_t Fdelete ( emuptr32_t fname );
int16_t Fdup ( int16_t handle );
int32_t Ffchmod ( int16_t fd, int16_t mode);
int32_t Ffchown ( int16_t fd, int16_t uid, int16_t gid );
int16_t Fforce ( int16_t stdh, int16_t nonstdh );
int16_t Ffstat64 (int16_t fd, /*struct stat */ uint32_t address);
int32_t Fgetchar ( int16_t fh, int16_t mode );
emuptr32_t Fgetdta ( void );
int32_t Finstat ( int16_t fh );
int32_t Flink ( emuptr32_t oldname, emuptr32_t newname );
int32_t Flock ( int16_t handle, int16_t mode, int32_t start, int32_t length );
int32_t Fmidipipe ( int16_t pid, int16_t in, int16_t out );
int32_t Fopen ( emuptr32_t fname, int16_t mode );
int32_t Foutstat ( int16_t fh );
int16_t Fpipe ( emuptr32_t usrh );
int32_t Fputchar ( int16_t fh, int32_t ch, int16_t mode );
int32_t Fread ( int16_t handle, int32_t count, emuptr32_t buf );
int32_t Freadlink ( int16_t bufsiz, emuptr32_t buf, emuptr32_t name );
int32_t Frename ( emuptr32_t oldname, emuptr32_t newname );
int32_t Fseek ( int32_t offset, int16_t handle, int16_t seekmode );
int32_t Fselect ( uint16_t timeout, emuptr32_t rfds, emuptr32_t wfds);
void Fsetdta ( emuptr32_t buf );
int32_t Fsfirst ( emuptr32_t filename, int16_t attr );
int16_t Fsnext ( void );
int16_t Fstat64 (uint16_t flag, uint32_t name, /*struct stat */ uint32_t address);
int32_t Fsymlink ( emuptr32_t oldname, emuptr32_t newname );
int32_t Fwrite ( int16_t handle, int32_t count, emuptr32_t buf );
int32_t Fxattr ( int16_t flag, emuptr32_t name, emuptr32_t xattr );

// Memory management

int32_t Maddalt ( emuptr32_t start, int32_t size );
emuptr32_t Malloc ( int32_t number );
int32_t Mfree ( emuptr32_t block );
int32_t Mshrink ( emuptr32_t block, int32_t newsiz );
emuptr32_t Mxalloc ( int32_t amount, int16_t mode );

// Network functions

int32_t Nversion( void );

// Process functions

void Pause ( void );
int16_t Pdomain ( int16_t dom );
int32_t Pexec ( uint16_t mode, emureg_t sp );
int16_t Pfork ( void );
int16_t Pgetauid ( void );
int32_t Pgetegid ( void );
int32_t Pgeteuid ( void );
int16_t Pgetgid ( void );
int32_t Pgetgroups( int16_t len, emuptr32_t gidset);
int16_t Pgetpgrp ( void );
int16_t Pgetpid ( void );
int16_t Pgetppid ( void );
int32_t Pgetpriority ( int16_t which, int16_t who);
int16_t Pgetuid ( void );
int16_t Pkill ( int16_t pid, int16_t sig );
int32_t Pmsg ( int16_t mode, int32_t mbox, emuptr32_t msg );
int16_t Pnice ( int16_t delta );
int32_t Prenice ( int16_t pid, int16_t delta );
void Prusage ( emuptr32_t r );
int32_t Psemaphore ( int16_t mode, int32_t id, int32_t timeout );
int16_t Psetauid ( int16_t id );
int32_t Psetegid ( int16_t egid );
int32_t Pseteuid ( int16_t euid );
int16_t Psetgid ( int16_t id );
int32_t Psetgroups( int16_t len, emuptr32_t gidset);
int32_t Psetlimit ( int16_t lim, int32_t value );
int16_t Psetpgrp ( int16_t pid, int16_t newgrp );
int32_t Psetpriority ( int16_t which, int16_t who, int16_t pri );
int32_t Psetregid ( int16_t rgid, int16_t egid);
int32_t Psetreuid ( int16_t ruid, int16_t euid);
int16_t Psetuid ( int16_t id );
int32_t Psigaction ( int16_t sig, emuptr32_t act, emuptr32_t oact );
int32_t Psigblock ( int32_t mask );
int32_t Psigintr ( int16_t vec, int16_t sig );
int32_t Psignal ( int16_t sig, int32_t handler );
void Psigpause ( int32_t mask );
int32_t Psigpending ( void );
void Psigreturn ( void );
int32_t Psigsetmask ( int32_t mask );
void Pterm ( uint16_t retcode );
void Pterm0 ( void );
void Ptermres ( int32_t keepcnt, int16_t retcode );
int16_t Ptrace(int16_t request, int16_t pid, emuptr32_t  addr, int32_t data);
int16_t Pumask ( int16_t mode );
int32_t Pusrval ( int32_t val );
int16_t Pvfork ( void );
int32_t Pwait ( void );
int32_t Pwait3 ( int16_t flag, emuptr32_t rusage );
int32_t Pwaitpid ( int16_t pid, int16_t flag, emuptr32_t rusage );

// System functions

int32_t STEFcntrl ( int16_t func, int32_t special1, int32_t special2 );
void Salert ( emuptr32_t msg );
int32_t Sconfig ( int16_t mode, int32_t flags );
void Shutdown ( int32_t mode );
int32_t Slbclose( emuptr32_t sl );
int32_t Slbopen( emuptr32_t name, emuptr32_t path, int32_t min_ver, emuptr32_t sl, emuptr32_t fn );
int32_t Srealloc ( int32_t len );
void Ssync ( void );
int32_t Ssystem ( int16_t mode, int32_t arg1, int32_t arg2 );
int32_t Super ( emuptr32_t stack );
int32_t Suptime ( emuptr32_t uptime, emuptr32_t loadaverage );
uint16_t Sversion ( void );
void Syield ( void );
int32_t Sysconf ( int16_t n );


// Data types
struct DTA
{
    int8_t    d_reserved[21];  /* Reserved for GEMDOS */
    uint8_t   d_attrib;        /* File attributes     */
    uint16_t  d_time;          /* Time                */
    uint16_t  d_date;          /* Date                */
    int32_t   d_length;        /* File length         */
    int8_t    d_fname[14];     /* Filename            */
} __attribute__((packed))__ ;

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
   uint32_t p_dta;          /* Pointer to the  DTA          */
                            /* Warning: Points first to the        */
                            /* command line !                      */
   uint32_t p_parent;       /* Pointer to the basepage of the      */
                            /* calling processes                   */
   int32_t  p_resrvd0;      /* Reserved                            */
   uint32_t p_env;          /* Address of the environment string   */
   int8_t    p_resrvd1[80]; /* Reserved                            */
   int8_t    p_cmdlin[128]; /* Command line                        */
} __attribute__((packed)) basepage_t;

extern emuptr32_t current_process; // Current process
