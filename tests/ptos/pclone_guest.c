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
	puts("Pclone guest modes: PASS");
	return 0;
}
