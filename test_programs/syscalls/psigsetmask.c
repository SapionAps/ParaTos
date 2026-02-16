#define _POSIX_C_SOURCE 200809L
#include <signal.h>

int main(void)
{
	#ifdef SIG_SETMASK
	sigset_t set;
	sigemptyset(&set);
	return sigprocmask(SIG_SETMASK, &set, 0) != 0;
	#else
	return 0;
	#endif
}
