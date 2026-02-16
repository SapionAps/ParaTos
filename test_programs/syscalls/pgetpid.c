#include <stdio.h>
#include <unistd.h>

int main(void)
{
	printf("%d\n", (int)getpid());
	return 0;
}
