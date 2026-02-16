#include <sys/stat.h>

int main(void)
{
	struct stat st;
	if (stat(".", &st) != 0) {
		return 1;
	}
	return chmod(".", st.st_mode);
}
