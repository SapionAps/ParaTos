/* An ordinary 68k PRG used as a loadable overlay, including a nested
 * Supexec when its parent already has an active guest supervisor stack. */
#include <mint/osbind.h>

static long supervisor(void)
{
	unsigned short sr;
	__asm__ volatile("move.w %%sr,%0" : "=d"(sr));
	return (sr & 0x2000) && Super(1L) == -1L ? 23 : 1;
}

int main(void)
{
	if (Super(1L) != 0 || Supexec(supervisor) != 23 || Super(1L) != 0) return 1;
	return 23;
}
