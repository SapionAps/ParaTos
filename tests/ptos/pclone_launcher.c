#include <mint/mintbind.h>
#include <mint/osbind.h>
#include <stdio.h>

int main(void)
{
	static char tail[128];
	puts("pTOS Paratos kernel integration: BEGIN");
	/* At #903's current head, a native Pexec(0) from the boot launcher
	 * can use an unmanaged continuation. Pexec(100) gives ParaTOS a
	 * scheduler task that can call Pclone. */
	long pid = Pexec(100, "C:\\PCGUEST.TOS", tail, 0L);
	long result = pid < 0 ? pid : Pwaitpid((short)pid, 0, 0L);
	if (pid <= 0 || result != pid << 16)
	{
		printf("Pclone guest launch PID %ld, wait result %ld\n", pid, result);
		puts("pTOS Paratos kernel integration: FAIL");
		return 1;
	}
	puts("pTOS Paratos kernel integration: PASS");
	return 0;
}
