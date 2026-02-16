#include <dirent.h>

int main(void)
{
	DIR *d = opendir(".");
	if (!d) {
		return 1;
	}
	return closedir(d);
}
