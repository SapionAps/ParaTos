#include <dirent.h>

int main(void)
{
	DIR *d = opendir(".");
	if (!d) {
		return 1;
	}
	(void)readdir(d);
	closedir(d);
	return 0;
}
