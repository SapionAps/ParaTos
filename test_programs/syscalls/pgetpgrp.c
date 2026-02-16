#include <stdio.h>
#include <unistd.h>

int main(void)
{
	printf("%d\n", (int)getpgrp());
	return 0;
}
