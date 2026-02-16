#include <fcntl.h>
#include <unistd.h>

int main(void)
{
	int fd = open("ptos_file.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (fd < 0) {
		return 1;
	}
	close(fd);

	fd = open("ptos_file.txt", O_RDONLY);
	if (fd < 0) {
		return 2;
	}
	return close(fd);
}
