#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
	int fd = open("ptos_rw.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (fd < 0) {
		return 1;
	}
	close(fd);

	if (rename("ptos_rw.txt", "ptos_rw_renamed.txt") != 0) {
		return 2;
	}
	if (rename("ptos_rw_renamed.txt", "ptos_rw.txt") != 0) {
		return 3;
	}
	return 0;
}
