#define _POSIX_C_SOURCE 200809L
#include <signal.h>

int main(void)
{
	#ifdef SIG_BLOCK
	sigset_t set;
	return sigpending(&set) != 0;
	#else
	return 0;
	#endif
}
