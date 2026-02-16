#include <stdio.h>
#include <unistd.h>

int main(void)
{
	printf("%d\n", (int)getgid());
	return 0;
}
