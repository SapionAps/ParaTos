#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "tasks.h"

static int locked;
static volatile sig_atomic_t handled;

void PosixTerminalLock(void) { assert(!locked); locked = 1; }
void PosixTerminalUnlock(void) { assert(locked); locked = 0; }

static void previous_handler(int number, siginfo_t *info, void *context)
{
	(void)context;
	assert(number == SIGTERM && info->si_signo == SIGTERM);
	sigset_t mask;
	sigprocmask(SIG_SETMASK, NULL, &mask);
	assert(sigismember(&mask, SIGUSR1));
	handled++;
}

int main(void)
{
	int master = posix_openpt(O_RDWR | O_NOCTTY);
	assert(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
	int slave = open(ptsname(master), O_RDWR | O_NOCTTY);
	assert(slave >= 0);
	struct sigaction original = {0}, action, actual;
	original.sa_sigaction = previous_handler;
	original.sa_flags = SA_SIGINFO | SA_RESETHAND;
	sigemptyset(&original.sa_mask);
	sigaddset(&original.sa_mask, SIGUSR1);
	assert(sigaction(SIGTERM, &original, NULL) == 0);
	action = original;
	action.sa_handler = SIG_IGN;
	action.sa_flags = 0;
	assert(sigaction(SIGINT, &action, NULL) == 0);

	int channel[2];
	assert(pipe(channel) == 0);
	assert(PosixTerminalBegin(channel[0]) == 0 && !locked);
	assert(PosixTerminalBegin(slave) == 1 && locked);
	assert(sigaction(SIGINT, NULL, &actual) == 0 && actual.sa_handler == SIG_IGN);
	assert(raise(SIGINT) == 0);
	struct termios terminal;
	assert(tcgetattr(slave, &terminal) == 0 && !(terminal.c_lflag & ICANON));
	assert(PosixTerminalEnd() == 0 && !locked);
	assert(sigaction(SIGTERM, NULL, &actual) == 0);
	assert(actual.sa_sigaction == previous_handler && (actual.sa_flags & SA_RESETHAND));
	assert(sigismember(&actual.sa_mask, SIGUSR1));

	assert(PosixTerminalBegin(slave) == 1);
	assert(raise(SIGTERM) == 0 && handled == 1);
	assert(tcgetattr(slave, &terminal) == 0);
	assert((terminal.c_lflag & (ICANON | ECHO)) == (ICANON | ECHO));
	assert(sigaction(SIGTERM, NULL, &actual) == 0 && actual.sa_handler == SIG_DFL);
	assert(PosixTerminalEnd() == 0 && !locked);
	assert(sigaction(SIGTERM, NULL, &actual) == 0 && actual.sa_handler == SIG_DFL);
	close(channel[0]); close(channel[1]); close(slave); close(master);
	return 0;
}
