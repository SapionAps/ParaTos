#include <sys/stat.h>

int main(void)
{
	mode_t old = umask(022);
	umask(old);
	return 0;
}
