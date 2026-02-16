#include <fcntl.h>
#include <unistd.h>

int main(void)
{
	const char msg[] = "read test\n";
	char buf[32];
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
	if (read(fd, buf, sizeof(buf)) < 0) {
		close(fd);
		return 4;
	}
	close(fd);
	return 0;
}
