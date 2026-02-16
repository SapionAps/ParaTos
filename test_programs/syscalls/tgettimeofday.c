#include <stdio.h>
#include <sys/time.h>

int main(void)
{
	struct timeval tv;
	if (gettimeofday(&tv, 0) != 0) {
		return 1;
	}
	printf("%ld %ld\n", (long)tv.tv_sec, (long)tv.tv_usec);
	return 0;
}
