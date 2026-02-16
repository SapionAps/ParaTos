#include <unistd.h>

int main(void)
{
	int p[2];
	if (pipe(p) != 0) {
		return 1;
	}
	close(p[0]);
	close(p[1]);
	return 0;
}
