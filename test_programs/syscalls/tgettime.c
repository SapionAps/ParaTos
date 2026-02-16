#include <stdio.h>
#include <time.h>

int main(void)
{
	time_t t = time(0);
	if (t == (time_t)-1) {
		return 1;
	}
	printf("%ld\n", (long)t);
	return 0;
}
