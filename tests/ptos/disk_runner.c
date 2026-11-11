/* Native boot driver for a copy of pTOS's m68k test disk. RUNTESTS.TOS
 * stays unmodified; the boot-configured binfmt handler selects Paratos. */
#include <stdio.h>
#include <mint/osbind.h>

int main(void)
{
	static char tail[128];
	puts("pTOS 68k test disk: BEGIN");
	long result = Pexec(0, "C:\\RUNTESTS.TOS", tail, 0);
	printf("68k RUNTESTS.TOS through Paratos: %ld\n", result);
	if (result)
	{
		puts("pTOS 68k test disk: FAIL");
		return 1;
	}
	puts("pTOS 68k test disk: PASS");
	return 0;
}
