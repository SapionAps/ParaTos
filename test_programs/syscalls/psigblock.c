#define _POSIX_C_SOURCE 200809L
#include <signal.h>

int main(void)
{
	#ifdef SIG_BLOCK
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	return sigprocmask(SIG_BLOCK, &set, 0) != 0;
	#else
	return 0;
	#endif
}
