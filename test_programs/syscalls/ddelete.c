#include <sys/stat.h>
#include <unistd.h>

int main(void)
{
	(void)mkdir("ptos_dcreate", 0777);
	return rmdir("ptos_dcreate");
}
