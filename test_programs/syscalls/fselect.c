#include <sys/select.h>
#include <unistd.h>

int main(void)
{
	int p[2];
	if (pipe(p) != 0) {
		return 1;
	}
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(p[0], &rfds);
	struct timeval tv = {0, 1000};
	int r = select(p[0] + 1, &rfds, 0, 0, &tv);
	close(p[0]);
	close(p[1]);
	return r < 0;
}
