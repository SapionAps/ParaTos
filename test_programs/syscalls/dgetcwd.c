#include <stdio.h>
#include <unistd.h>

int main(void)
{
	char buf[256];
	if (!getcwd(buf, sizeof(buf))) {
		return 1;
	}
	puts(buf);
	return 0;
}
