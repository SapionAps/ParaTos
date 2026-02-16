#include <poll.h>
#include <unistd.h>

int main(void)
{
	int p[2];
	if (pipe(p) != 0) {
		return 1;
	}
	struct pollfd fd;
	fd.fd = p[0];
	fd.events = POLLIN;
	fd.revents = 0;
	int r = poll(&fd, 1, 1);
	close(p[0]);
	close(p[1]);
	return r < 0;
}
