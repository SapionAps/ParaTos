#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <unistd.h>

ssize_t readlink(const char *path, char *buf, size_t bufsiz);
int symlink(const char *target, const char *linkpath);

int main(void)
{
	int fd = open("ptos_link_src.txt", O_CREAT | O_TRUNC | O_WRONLY, 0666);
	if (fd < 0) {
		return 1;
	}
	close(fd);

	(void)unlink("ptos_symlink.txt");
	if (symlink("ptos_link_src.txt", "ptos_symlink.txt") != 0) {
		return 2;
	}

	char buf[256];
	return readlink("ptos_symlink.txt", buf, sizeof(buf)) < 0 ? 3 : 0;
}
