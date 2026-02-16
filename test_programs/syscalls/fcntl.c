#include <fcntl.h>
#include <unistd.h>

int main(void)
{
	int fd = open("ptos_rw.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
	if (fd < 0) {
		return 1;
	}
	if (fcntl(fd, F_GETFL) < 0) {
		close(fd);
		return 2;
	}
	close(fd);
	return 0;
}
