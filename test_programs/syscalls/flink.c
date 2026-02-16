#include <fcntl.h>
#include <unistd.h>

int main(void)
{
	int fd = open("ptos_link_src.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (fd < 0) {
		return 1;
	}
	close(fd);
	(void)unlink("ptos_link.txt");
	if (link("ptos_link_src.txt", "ptos_link.txt") != 0) {
		return 2;
	}
	return 0;
}
