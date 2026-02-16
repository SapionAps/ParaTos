#include <unistd.h>

int main(void)
{
	(void)unlink("ptos_file.txt");
	(void)unlink("ptos_rw.txt");
	(void)unlink("ptos_link.txt");
	(void)unlink("ptos_symlink.txt");
	return 0;
}
