#include <mint/osbind.h>
#include <stdio.h>

int main(void)
{
	static char tail[128];
	puts("pTOS Paratos kernel integration: BEGIN");
	long result = Pexec(0, "C:\\PCGUEST.TOS", tail, 0L);
	if (result != 0)
	{
		printf("Pclone guest mode 0 result %ld\n", result);
		puts("pTOS Paratos kernel integration: FAIL");
		return 1;
	}
	puts("pTOS Paratos kernel integration: PASS");
	return 0;
}
