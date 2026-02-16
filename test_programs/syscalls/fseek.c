#include <fcntl.h>
#include <unistd.h>

int main(void)
{
	const char msg[] = "seek test\n";
	int fd = open("ptos_rw.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (fd < 0) {
		return 1;
	}
	if (write(fd, msg, sizeof(msg) - 1) < 0) {
		close(fd);
		return 2;
	}
	close(fd);

	fd = open("ptos_rw.txt", O_RDONLY);
	if (fd < 0) {
		return 3;
	}
	if (lseek(fd, 0, SEEK_SET) < 0) {
		close(fd);
		return 4;
	}
	close(fd);
	return 0;
}
