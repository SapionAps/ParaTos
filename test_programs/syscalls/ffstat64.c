#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
	struct stat st;
	int fd = open("ptos_stat.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (fd < 0) {
		return 1;
	}
	close(fd);

	fd = open("ptos_stat.txt", O_RDONLY);
	if (fd < 0) {
		return 2;
	}
	int r = fstat(fd, &st);
	close(fd);
	return r != 0;
}
