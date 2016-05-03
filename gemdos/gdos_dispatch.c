#include "common.h"
#include "m68k.h"
#include "m68kcpu.h"
#include "gdos.h"

void dispatch_gdos_trap()
{
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
	uint16_t num = m68k_read_memory_16(sp);
	int32_t retval = 0;

	switch (num) {
	case    0	:
		Pterm0();
		break;
	case    1	:
		retval = Cconin();
		break;
	case    2	:
		retval = Cconout(m68k_read_memory_16(sp+2));
		break;
	case    3	:
		retval = Cauxin();
		break;
	case    4	:
		retval = Cauxout(m68k_read_memory_16(sp+2));
		break;
	case    5	:
		retval = Cprnout(m68k_read_memory_16(sp+2));
		break;
	case    6	:
		retval = Crawio(m68k_read_memory_16(sp+2));
		break;
	case    7	:
		retval = Crawcin();
		break;
	case    8	:
		retval = Cnecin();
		break;
	case    9	:
		retval = Cconws(m68k_read_memory_32(sp+2));
		break;
	case   10	:
		retval = Cconrs(m68k_read_memory_32(sp+2));
		break;
	case   11	:
		retval = Cconis();
		break;
	case   14	:
		retval = Dsetdrv(m68k_read_memory_16(sp+2));
		break;
	case   16	:
		retval = Cconos();
		break;
	case   17	:
		retval = Cprnos();
		break;
	case   18	:
		retval = Cauxis();
		break;
	case   19	:
		retval = Cauxos();
		break;
	case   20	:
		retval = Maddalt(m68k_read_memory_32(sp+2), m68k_read_memory_32(sp+6));
		break;
	case   21	:
		retval = Srealloc(m68k_read_memory_32(sp+2));
		break;
	case   22	:
		retval = Slbopen(m68k_read_memory_32(sp+2), m68k_read_memory_32(sp+6), m68k_read_memory_32(sp+10), m68k_read_memory_32(sp+14), m68k_read_memory_32(sp+18));
		break;
	case   23	:
		retval = Slbclose(m68k_read_memory_32(sp+2));
		break;
	case   25	:
		retval = Dgetdrv();
		break;
	case   26	:
		Fsetdta(m68k_read_memory_32(sp+2));
		break;
	case   32	:
		retval = Super(m68k_read_memory_32(sp+2));
		break;
	case   42	:
		retval = Tgetdate();
		break;
	case   43	:
		retval = Tsetdate(m68k_read_memory_16(sp+2));
		break;
	case   44	:
		retval = Tgettime();
		break;
	case   45	:
		retval = Tsettime(m68k_read_memory_16(sp+2));
		break;
	case   47	:
		retval = Fgetdta();
		break;
	case   48	:
		retval = Sversion();
		break;
	case   49	:
		Ptermres(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6));
		break;
	case   51	:
		retval = Sconfig(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4));
		break;
	case   54	:
		retval = Dfree(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6));
		break;
	case   57	:
		retval = Dcreate(m68k_read_memory_32(sp+2));
		break;
	case   58	:
		retval = Ddelete(m68k_read_memory_32(sp+2));
		break;
	case   59	:
		retval = Dsetpath(m68k_read_memory_32(sp+2));
		break;
	case   60	:
		retval = Fcreate(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6));
		break;
	case   61	:
		retval = Fopen(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6));
		break;
	case   62	:
		retval = Fclose(m68k_read_memory_16(sp+2));
		break;
	case   63	:
		retval = Fread(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case   64	:
		retval = Fwrite(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case   65	:
		retval = Fdelete(m68k_read_memory_32(sp+2));
		break;
	case   66	:
		retval = Fseek(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6), m68k_read_memory_16(sp+8));
		break;
	case   67	:
		retval = Fattrib(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6), m68k_read_memory_16(sp+8));
		break;
	case   68	:
		retval = Mxalloc(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6));
		break;
	case   69	:
		retval = Fdup(m68k_read_memory_16(sp+2));
		break;
	case   70	:
		retval = Fforce(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4));
		break;
	case   71	:
		retval = Dgetpath(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6));
		break;
	case   72	:
		retval = Malloc(m68k_read_memory_32(sp+2));
		break;
	case   73	:
		retval = Mfree(m68k_read_memory_32(sp+2));
		break;
	case   74	:
		retval = Mshrink(m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case   75	:
		retval = Pexec(m68k_read_memory_16(sp+2), sp+4);
		break;
	case   76	:
		Pterm(m68k_read_memory_16(sp+2));
		break;
	case   78	:
		retval = Fsfirst(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6));
		break;
	case   79	:
		retval = Fsnext();
		break;
	case   86	:
		retval = Frename(m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case   87	:
		Fdatime(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6), m68k_read_memory_16(sp+8));
		break;
	case   92	:
		retval = Flock(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4), m68k_read_memory_32(sp+6), m68k_read_memory_32(sp+10));
		break;
	case   96	:
		retval = Nversion();
		break;
	case  255	:
		Syield();
		break;
	case  256	:
		retval = Fpipe(m68k_read_memory_32(sp+2));
		break;
	case  257	:
		retval = Ffchown(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4), m68k_read_memory_16(sp+6));
		break;
	case  258	:
		retval = Ffchmod(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4));
		break;
	case  260	:
		retval = Fcntl(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_16(sp+8));
		break;
	case  261	:
		retval = Finstat(m68k_read_memory_16(sp+2));
		break;
	case  262	:
		retval = Foutstat(m68k_read_memory_16(sp+2));
		break;
	case  263	:
		retval = Fgetchar(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4));
		break;
	case  264	:
		retval = Fputchar(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_16(sp+8));
		break;
	case  265	:
		retval = Pwait();
		break;
	case  266	:
		retval = Pnice(m68k_read_memory_16(sp+2));
		break;
	case  267	:
		retval = Pgetpid();
		break;
	case  268	:
		retval = Pgetppid();
		break;
	case  269	:
		retval = Pgetpgrp();
		break;
	case  270	:
		retval = Psetpgrp(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4));
		break;
	case  271	:
		retval = Pgetuid();
		break;
	case  272	:
		retval = Psetuid(m68k_read_memory_16(sp+2));
		break;
	case  273	:
		retval = Pkill(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4));
		break;
	case  274	:
		retval = Psignal(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4));
		break;
	case  275	:
		retval = Pvfork();
		break;
	case  276	:
		retval = Pgetgid();
		break;
	case  277	:
		retval = Psetgid(m68k_read_memory_16(sp+2));
		break;
	case  278	:
		retval = Psigblock(m68k_read_memory_32(sp+2));
		break;
	case  279	:
		retval = Psigsetmask(m68k_read_memory_32(sp+2));
		break;
	case  280	:
		retval = Pusrval(m68k_read_memory_32(sp+2));
		break;
	case  281	:
		retval = Pdomain(m68k_read_memory_16(sp+2));
		break;
	case  282	:
		Psigreturn();
		break;
	case  283	:
		retval = Pfork();
		break;
	case  284	:
		retval = Pwait3(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4));
		break;
	case  285	:
		retval = Fselect(m68k_read_memory_32(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case  286	:
		Prusage(m68k_read_memory_32(sp+2));
		break;
	case  287	:
		retval = Psetlimit(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4));
		break;
	case  288	:
		retval = Talarm(m68k_read_memory_32(sp+2));
		break;
	case  289	:
		Pause();
		break;
	case  290	:
		retval = Sysconf(m68k_read_memory_16(sp+2));
		break;
	case  291	:
		retval = Psigpending();
		break;
	case  292	:
		retval = Dpathconf(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6));
		break;
	case  293	:
		retval = Pmsg(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case  294	:
		retval = Fmidipipe(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4), m68k_read_memory_16(sp+6));
		break;
	case  295	:
		retval = Prenice(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4));
		break;
	case  296	:
		retval = Dopendir(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6));
		break;
	case  297	:
		retval = Dreaddir(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case  298	:
		retval = Drewinddir(m68k_read_memory_32(sp+2));
		break;
	case  299	:
		retval = Dclosedir(m68k_read_memory_32(sp+2));
		break;
	case  300	:
		retval = Fxattr(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case  301	:
		retval = Flink(m68k_read_memory_32(sp+2), m68k_read_memory_32(sp+6));
		break;
	case  302	:
		retval = Fsymlink(m68k_read_memory_32(sp+2), m68k_read_memory_32(sp+6));
		break;
	case  303	:
		retval = Freadlink(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case  304	:
		retval = Dcntl(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case  305	:
		retval = Fchown(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6), m68k_read_memory_16(sp+8));
		break;
	case  306	:
		retval = Fchmod(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6));
		break;
	case  307	:
		retval = Pumask(m68k_read_memory_16(sp+2));
		break;
	case  308	:
		retval = Psemaphore(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case  309	:
		retval = Dlock(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4));
		break;
	case  310	:
		Psigpause(m68k_read_memory_32(sp+2));
		break;
	case  311	:
		retval = Psigaction(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case  312	:
		retval = Pgeteuid();
		break;
	case  313	:
		retval = Pgetegid();
		break;
	case  314	:
		retval = Pwaitpid(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4), m68k_read_memory_32(sp+6));
		break;
	case  315	:
		retval = Dgetcwd(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6), m68k_read_memory_16(sp+8));
		break;
	case  316	:
		Salert(m68k_read_memory_32(sp+2));
		break;
	case  317	:
		retval = Tmalarm(m68k_read_memory_32(sp+2));
		break;
	case  318	:
		retval = Psigintr(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4));
		break;
	case  319	:
		retval = Suptime(m68k_read_memory_32(sp+2), m68k_read_memory_32(sp+6));
		break;
	case  322	:
		retval = Dxreaddir(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8), m68k_read_memory_32(sp+12), m68k_read_memory_32(sp+16));
		break;
	case  323	:
		retval = Pseteuid(m68k_read_memory_16(sp+2));
		break;
	case  324	:
		retval = Psetegid(m68k_read_memory_16(sp+2));
		break;
	case  325	:
		retval = Psetauid(m68k_read_memory_16(sp+2));
		break;
	case  326	:
		retval = Pgetauid();
		break;
	case  327	:
		retval = Pgetgroups(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4));
		break;
	case  328	:
		retval = Psetgroups(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4));
		break;
	case  329	:
		retval = Tsetitimer(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8), m68k_read_memory_32(sp+12), m68k_read_memory_32(sp+16));
		break;
	case  330	:
		retval = Dchroot(m68k_read_memory_32(sp+2));
		break;
	case 331:
		retval = Fstat64(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case  334	:
		retval = Psetreuid(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4));
		break;
	case  335	:
		retval = Psetregid(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4));
		break;
	case  336	:
		Ssync();
		break;
	case  337	:
		Shutdown(m68k_read_memory_32(sp+2));
		break;
	case  338	:
		retval = Dreadlabel(m68k_read_memory_32(sp+2), m68k_read_memory_32(sp+6), m68k_read_memory_16(sp+10));
		break;
	case  339	:
		retval = Dwritelabel(m68k_read_memory_32(sp+2), m68k_read_memory_32(sp+6));
		break;
	case  340	:
		retval = Ssystem(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	case  341	:
		retval = Tgettimeofday(m68k_read_memory_32(sp+2), m68k_read_memory_32(sp+6));
		break;
	case  342	:
		retval = Tsettimeofday(m68k_read_memory_32(sp+2), m68k_read_memory_32(sp+6));
		break;
	case  344	:
		retval = Pgetpriority(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4));
		break;
	case  345	:
		retval = Psetpriority(m68k_read_memory_16(sp+2), m68k_read_memory_16(sp+4), m68k_read_memory_16(sp+6));
		break;
	case 349:
		retval = Ffstat64(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4));
		break;
	case  1296	:
		retval = Dxopendir(m68k_read_memory_32(sp+2), m68k_read_memory_16(sp+6));
		break;
	case  24000	:
		retval = STEFcntrl(m68k_read_memory_16(sp+2), m68k_read_memory_32(sp+4), m68k_read_memory_32(sp+8));
		break;
	default:
		retval = TOS_ENOSYS;
	}
	m68k_set_reg(M68K_REG_D0, retval);
}
