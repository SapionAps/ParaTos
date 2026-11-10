/* An ordinary 68k PRG used as a loadable overlay, including a nested
 * Supexec when its parent already has an active guest supervisor stack. */
#include <mint/osbind.h>
#include <mint/basepage.h>
#include <string.h>
#include <stdio.h>

static long supervisor(void)
{
	unsigned short sr;
	__asm__ volatile("move.w %%sr,%0" : "=d"(sr));
	return (sr & 0x2000) && Super(1L) == -1L ? 23 : 1;
}

int main(void)
{
	if (Super(1L) != 0 || Supexec(supervisor) != 23 || Super(1L) != 0) return 1;
	char cwd[128];
	if (Dgetpath(cwd, 3)) return 5;
	const char *name = cwd;
	while (*name == '\\') name++;
	if (strcmp(name, "OVLTEST")) { fprintf(stderr, "overlay inherited directory: [%s]\n", cwd); return 6; }
	(void)Cconws("loaded");
	short inherited = (unsigned char)_base->p_cmdlin[1];
	if (Fclose(inherited)) return 3;
	long fd = Fcreate("C:/OVLTEST/LEAK.TMP", 0), copy = Fdup(1);
	if (fd < 0 || copy < 0 || Fforce(1, (short)fd) || Fforce(1, (short)fd)) return 4;
	(void)Cconws("overlay");
	/* Raw BIOS output must bypass the redirected GEMDOS stdout. */
	(void)Bconout(2, 'B');
	/* Intentionally leave both handles and stdout open at Pterm. */
	_base->p_cmdlin[1] = fd; _base->p_cmdlin[2] = copy;
	if (Dsetpath("C:/OVLTEST")) return 2;
	Dsetdrv(0);
	return 23;
}
