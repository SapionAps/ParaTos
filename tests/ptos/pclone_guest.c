/* Ordinary 68k code. The child runs directly from a staged basepage and
 * shares the caller's globals, so an early join would deadlock. */
#include <mint/basepage.h>
#include <mint/mintbind.h>
#include <mint/osbind.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char tail[128];
static volatile long allow_exit, child_pid, child_parent;
static short inherited;
static int check_pid;
static _DTA search_dta;
static char pattern[] = "C:\\PCSTATE\\*.TMP";

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
	if (check_pid)
	{
		/* Binding the parent's guest DTA address cannot import its cursor. */
		Fsetdta(&search_dta);
		if (Fsnext() != -49L || Fsfirst(pattern, 0) || Fsnext() || Fsnext() != -49L)
			Pterm(96);
		char command[128] = {0}, parent[16] = {0};
		long fd = Fcreate("C:\\PCSTATE\\PID.TMP", 0);
		if (fd < 0) Pterm(97);
		command[0] = snprintf(command + 1, sizeof(command) - 1, "pid:%ld", fd);
		long pid = Pexec(100, "C:\\NATIVE.TOS", command, 0L);
		if (pid <= 0 || Pwaitpid((short)pid, 0, 0L) != (pid << 16) ||
			Fseek(0, (short)fd, 0) || Fread((short)fd, sizeof(parent) - 1, parent) <= 0 ||
			atol(parent) != child_pid || Fclose((short)fd) || Fdelete("C:\\PCSTATE\\PID.TMP"))
			Pterm(98);
	}
	Pterm(0x1234);
}

static int check(short mode)
{
	const int asynchronous = mode >= 100;
	const int release = mode == 6 || mode == 106;
	const long available = (long)Malloc(-1);
	allow_exit = !asynchronous;
	check_pid = mode == 106;
	child_pid = child_parent = 0;
	long saved_stdout = Fdup(1);
	long file = Fcreate("C:\\PCSTATE\\CLONE.TMP", 0);
	long forced = saved_stdout < 0 || file < 0 ? -1 : Fforce(1, (short)file);
	long changed = forced ? -1 : Dsetpath("C:\\PCSTATE");
	if (saved_stdout < 0 || file < 0 || forced || changed)
	{
		fprintf(stderr, "Pclone prepare failed: std=%ld file=%ld force=%ld cwd=%ld\n",
			saved_stdout, file, forced, changed);
		return 8;
	}
	inherited = (short)file;
	long observer = mode == 104 ? Fdup(1) : file;
	if (observer < 0) return 8;
	if (check_pid)
	{
		long other = Fcreate("C:\\PCSTATE\\OTHER.TMP", 0);
		Fsetdta(&search_dta);
		if (other < 0 || Fclose((short)other) || Fsfirst(pattern, 0)) return 14;
	}
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
		if (mode == 104 && Fclose(inherited)) return 10;
		allow_exit = 1;
		long status = Pwait3(0, 0L);
		if (status != ((result << 16) | 0x34) || child_pid != result ||
			Pwaitpid((short)result, 1, 0L) != -21L) return 3;
	}
	else if (result != 0x1234) return 4;
	if (child_pid <= 0 || child_pid == Pgetpid() || child_parent != Pgetpid()) return 5;
	if (check_pid && (Fsnext() || Fsnext() != -49L || Fdelete("C:\\PCSTATE\\OTHER.TMP"))) return 14;
	char dir[128];
	if (Dgetpath(dir, 3) || dir[dir[0] == '\\'] || Fseek(0, (short)observer, 1) != 2 ||
		Fclose((short)observer) || Fdelete("C:\\PCSTATE\\CLONE.TMP")) return 10;
	if (release ? Mfree(bp) != -40L : Mfree(env) || Mfree(bp)) return 6;
	return (long)Malloc(-1) == available ? 0 : 7;
}

static void orphan(void)
{
	child_pid = Pgetpid();
	while (!allow_exit) Syield();
	child_parent = Pgetppid();
	Pterm(0);
}

static void orphan_parent(void)
{
	Pterm(start(orphan, 106, 0) > 0 ? 0 : 1);
}

static int check_orphans(void)
{
	const long available = (long)Malloc(-1);
	for (int i = 0; i < 3; i++)
	{
		allow_exit = child_pid = 0;
		child_parent = -1;
		long parent = start(orphan_parent, 106, 1);
		if (parent <= 0 || Pwaitpid((short)parent, 0, 0L) != (parent << 16)) return 1;
		while (!child_pid) Syield();
		if (child_pid == parent || Pwaitpid((short)child_pid, 1, 0L) != -21L)
			return 2;
		allow_exit = 1;
		while ((long)Malloc(-1) != available) Syield();
		if (child_parent != 0) return 3;
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
			fprintf(stderr, "Pclone guest mode %d failed at check %d\n", modes[i], result);
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
