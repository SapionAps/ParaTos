/* Native replacement for RUNTESTS.TOS on a copy of pTOS's m68k test disk.
 * The original 68k executable is preserved as GUESTEST.TOS. Explicitly
 * prepend the native emulator; no binfmt registration is needed. */
#include <stdio.h>
#include <mint/osbind.h>

int main(void)
{
	static char tail[128] = {127, 0};
	static const char environment[] = "ARGV=\0C:\\PARATOS.PRG\0C:\\GUESTEST.TOS\0";
	puts("pTOS 68k test disk: BEGIN");
	long result = Pexec(0, "C:\\PARATOS.PRG", tail, environment);
	printf("68k RUNTESTS.TOS through Paratos: %ld\n", result);
	if (result)
	{
		puts("pTOS 68k test disk: FAIL");
		return 1;
	}
	puts("pTOS 68k test disk: PASS");
	return 0;
}
