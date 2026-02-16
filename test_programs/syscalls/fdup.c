#include <fcntl.h>
#include <unistd.h>

int main(void)
{
	int fd = open("ptos_rw.txt", O_CREAT | O_TRUNC | O_RDWR, 0666);
	if (fd < 0) {
		return 1;
	}
	int d = dup(fd);
	close(fd);
	if (d < 0) {
		return 2;
	}
	close(d);
	return 0;
}
