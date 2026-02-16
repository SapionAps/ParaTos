#include <stdio.h>
#include <unistd.h>

int main(void)
{
	printf("%d\n", (int)getuid());
	return 0;
}
