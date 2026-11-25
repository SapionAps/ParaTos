#include <mint/osbind.h>
#include <stdio.h>

int main(void)
{
	char *argv[] = {"C:\\PCGUEST.TOS", NULL};
	char *envp[] = {NULL};
	puts("pTOS Paratos kernel integration: BEGIN");
	long result = Pexecve(0, argv[0], argv, envp);
	if (result != 0)
	{
		printf("Pclone guest mode 0 result %ld\n", result);
		puts("pTOS Paratos kernel integration: FAIL");
		return 1;
	}
	puts("pTOS Paratos kernel integration: PASS");
	return 0;
}
