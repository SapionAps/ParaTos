/* Ordinary 68k code. The child runs directly from a staged basepage and
 * shares the caller's globals, so an early join would deadlock. */
#include <mint/basepage.h>
#include <mint/mintbind.h>
#include <mint/osbind.h>
#include <stdio.h>
#include <string.h>

static char tail[128];
static volatile long allow_exit, child_pid, child_parent;
static short inherited;

static long start(void (*entry)(void), short mode, int compact)
{
	long result = Pexec(5, 0L, tail, 0L);
	if (result < 0) return result;
	BASEPAGE *bp = (BASEPAGE *)result;
	void *env = bp->p_env;
	bp->p_tbase = (void *)entry;
	result = compact ? Mshrink(bp, 4096) : 0;
	if (!result) result = Pexec(mode, 0L, bp, 0L);
	if (result < 0)
	{
		Mfree(env);
		Mfree(bp);
	}
	return result;
}

static void child(void)
{
	char dir[128];
	child_pid = Pgetpid();
	child_parent = Pgetppid();
	while (!allow_exit) Syield();
	if (Dgetpath(dir, 3) || strcmp(dir + (dir[0] == '\\'), "PCSTATE") ||
		Fwrite(inherited, 1, "c") != 1 || Fclose(inherited) ||
		Fwrite(1, 1, "s") != 1 || Dsetpath("C:\\"))
		Pterm(99);
	Pterm(0x1234);
}

static int check(short mode)
{
	const int asynchronous = mode >= 100;
	const int release = mode == 6 || mode == 106;
	const long available = (long)Malloc(-1);
	allow_exit = !asynchronous;
	child_pid = child_parent = 0;
	long saved_stdout = Fdup(1);
	long file = Fcreate("C:\\PCSTATE\\CLONE.TMP", 0);
	if (saved_stdout < 0 || file < 0 || Fforce(1, (short)file) || Dsetpath("C:\\PCSTATE"))
		return 8;
	inherited = (short)file;
	long result;
	if (mode == 4)
		result = Pexec(3, "C:\\PCGUEST.TOS", tail, 0L);
	else if (mode == 6)
		result = Pexec(7, 0L, tail, 0L);
	else
		result = Pexec(5, 0L, tail, 0L);
	if (result < 0) return 1;
	if (Fforce(1, (short)saved_stdout) || Fclose((short)saved_stdout) || Dsetpath("C:\\"))
		return 9;
	BASEPAGE *bp = (BASEPAGE *)result;
	bp->p_tbase = (void *)child;
	void *env = bp->p_env;
	result = Pexec(mode, 0L, bp, 0L);
	if (asynchronous)
	{
		if (result <= 0 || Pwaitpid((short)result, 1, 0L) != 0 ||
			Mfree(bp) != -40L) return 2;
		allow_exit = 1;
		long status = Pwait3(0, 0L);
		if (status != ((result << 16) | 0x34) || child_pid != result ||
			Pwaitpid((short)result, 1, 0L) != -21L) return 3;
	}
	else if (result != 0x1234) return 4;
	if (child_pid <= 0 || child_pid == Pgetpid() || child_parent != Pgetpid()) return 5;
	char dir[128];
	if (Dgetpath(dir, 3) || dir[dir[0] == '\\'] || Fseek(0, inherited, 1) != 2 ||
		Fclose(inherited) || Fdelete("C:\\PCSTATE\\CLONE.TMP")) return 10;
	if (release ? Mfree(bp) != -40L : Mfree(env) || Mfree(bp)) return 6;
	return (long)Malloc(-1) == available ? 0 : 7;
}

static void orphan(void)
{
	child_pid = Pgetpid();
	while (!allow_exit) Syield();
	Pterm(0);
}

static void orphan_parent(void)
{
	Pterm(start(orphan, 106, 0) > 0 ? 0 : 1);
}

static int check_orphans(void)
{
	const long available = (long)Malloc(-1);
	long previous = 0;
	for (int i = 0; i < 3; i++)
	{
		allow_exit = child_pid = 0;
		long parent = start(orphan_parent, 106, 1);
		if (parent <= 0 || Pwaitpid((short)parent, 0, 0L) != (parent << 16)) return 1;
		while (!child_pid) Syield();
		if ((previous && child_pid != previous) || Pwaitpid((short)child_pid, 1, 0L) != -21L)
			return 2;
		previous = child_pid;
		allow_exit = 1;
		while ((long)Malloc(-1) != available) Syield();
	}
	return 0;
}

static void shutdown_child(void)
{
	child_pid = Pgetpid();
	while (!allow_exit) Syield();
	Pterm(23);
}

static void shutdown_parent(void)
{
	long result = start(shutdown_child, 6, 0);
	if (result != 23)
	{
		printf("Pclone shutdown child returned %ld\n", result);
		Pterm(1);
	}
	puts("Pclone shutdown: PASS");
	Pterm(0);
}

int main(void)
{
	static const short modes[] = {4, 6, 104, 106};
	if (Dcreate("C:\\PCSTATE")) return 11;
	for (unsigned int i = 0; i < sizeof(modes) / sizeof(modes[0]); i++)
	{
		int result = check(modes[i]);
		if (result)
		{
			printf("Pclone guest mode %d failed at check %d\n", modes[i], result);
			return result;
		}
	}
	if (Ddelete("C:\\PCSTATE")) return 12;
	int orphan_result = check_orphans();
	if (orphan_result)
	{
		printf("Pclone orphan check failed at %d\n", orphan_result);
		return 13;
	}
	puts("Pclone guest modes: PASS");

	allow_exit = child_pid = 0;
	if (start(shutdown_parent, 106, 1) <= 0) return 15;
	while (!child_pid) Syield();
	allow_exit = 1;
	Pterm(0);
	return 0;
}
