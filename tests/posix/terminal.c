#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include "tasks.h"

static int locked, slave;
static atomic_int handled, queued;
static atomic_int reading;

void PosixTerminalLock(void) { assert(!locked); locked = 1; }
void PosixTerminalUnlock(void) { assert(locked); locked = 0; }

static void previous_handler(int number, siginfo_t *info, void *context)
{
	assert(number == SIGTERM && info->si_signo == SIGTERM && context);
	assert(info->si_pid == getpid() && info->si_uid == getuid());
#ifdef __linux__
	if (queued)
	{
		assert(info->si_code == SI_QUEUE);
		assert(info->si_value.sival_int == 0x12345678);
	}
#endif
	sigset_t mask;
	sigprocmask(SIG_SETMASK, NULL, &mask);
	assert(sigismember(&mask, SIGUSR1));
	assert(!sigismember(&mask, SIGTTOU)); /* only the user's mask applies */
	struct termios terminal;
	assert(tcgetattr(slave, &terminal) == 0);
	assert(!(terminal.c_lflag & (ICANON | ECHO)));
	handled++;
}

static void *read_character(void *argument)
{
	(void)argument;
	atomic_store(&reading, 1);
	char byte;
	ssize_t count;
	do { count = read(slave, &byte, 1); } while (count < 0 && errno == EINTR);
	assert(count == 1 && byte == 'Q');
	return NULL;
}

static void check_background_cleanup(void)
{
	pid_t controller = fork();
	assert(controller >= 0);
	if (!controller)
	{
		assert(setsid() >= 0);
		int master = posix_openpt(O_RDWR | O_NOCTTY);
		assert(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
		int terminal = open(ptsname(master), O_RDWR | O_NOCTTY);
		assert(terminal >= 0 && ioctl(terminal, TIOCSCTTY, 0) == 0);
		/* The controller manipulates foreground groups; the reader must have
		 * the default SIGTTOU action to detect interrupted background cleanup. */
		signal(SIGTTOU, SIG_IGN);
		int ready[2], start[2];
		assert(pipe(ready) == 0 && pipe(start) == 0);
		pid_t reader = fork();
		assert(reader >= 0);
		if (!reader)
		{
			assert(setpgid(0, 0) == 0);
			signal(SIGTTOU, SIG_DFL);
			char byte;
			assert(write(ready[1], "G", 1) == 1);
			assert(read(start[0], &byte, 1) == 1);
			assert(PosixTerminalBegin(terminal) == 1);
			assert(write(ready[1], "R", 1) == 1);
			for (;;) pause();
		}
		close(ready[1]); close(start[0]);
		char byte;
		assert(read(ready[0], &byte, 1) == 1 && byte == 'G');
		assert(tcsetpgrp(terminal, reader) == 0);
		assert(write(start[1], "S", 1) == 1);
		assert(read(ready[0], &byte, 1) == 1 && byte == 'R');
		assert(tcsetpgrp(terminal, getpgrp()) == 0);
		assert(kill(reader, SIGTERM) == 0);
		int status;
		assert(waitpid(reader, &status, WUNTRACED) == reader);
		if (WIFSTOPPED(status))
		{
			kill(reader, SIGKILL);
			waitpid(reader, NULL, 0);
		}
		assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM);
		struct termios restored;
		assert(tcgetattr(terminal, &restored) == 0);
		assert((restored.c_lflag & (ICANON | ECHO)) == (ICANON | ECHO));
		_exit(0);
	}
	int status;
	assert(waitpid(controller, &status, 0) == controller);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(void)
{
	int master = posix_openpt(O_RDWR | O_NOCTTY);
	assert(master >= 0 && grantpt(master) == 0 && unlockpt(master) == 0);
	slave = open(ptsname(master), O_RDWR | O_NOCTTY);
	assert(slave >= 0);
	struct sigaction original = {0}, action, actual;
	original.sa_sigaction = previous_handler;
	original.sa_flags = SA_SIGINFO;
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
	assert(raise(SIGTERM) == 0 && handled == 1);
	assert(tcgetattr(slave, &terminal) == 0 && !(terminal.c_lflag & (ICANON | ECHO)));

	/* Deliver on this thread while another host thread is blocked in read.
	 * A single byte must still complete that read without echo or newline. */
	pthread_t reader;
	assert(pthread_create(&reader, NULL, read_character, NULL) == 0);
	while (!atomic_load(&reading)) {}
	struct timespec delay = { .tv_nsec = 20000000 };
	nanosleep(&delay, NULL);
	assert(raise(SIGTERM) == 0 && handled == 2);
#ifdef __linux__
	queued = 1;
	union sigval value = { .sival_int = 0x12345678 };
	assert(sigqueue(getpid(), SIGTERM, value) == 0);
	/* The process-directed signal may run on the reader thread. */
	while (handled != 3) {}
	queued = 0;
#endif
	assert(write(master, "Q", 1) == 1);
	assert(pthread_join(reader, NULL) == 0);
	assert(PosixTerminalEnd() == 0 && !locked);
	assert(tcgetattr(slave, &terminal) == 0);
	assert((terminal.c_lflag & (ICANON | ECHO)) == (ICANON | ECHO));
	assert(sigaction(SIGTERM, NULL, &actual) == 0);
	assert(actual.sa_sigaction == previous_handler && (actual.sa_flags & SA_SIGINFO));
	assert(sigismember(&actual.sa_mask, SIGUSR1));

	original.sa_flags |= SA_RESETHAND;
	assert(sigaction(SIGTERM, &original, NULL) == 0);
	assert(sigaction(SIGTERM, NULL, &actual) == 0);
	int one_shot_flags = actual.sa_flags;
	assert(PosixTerminalBegin(slave) == 1);
	int before = handled;
	assert(raise(SIGTERM) == 0);
	assert(handled == before + 1);
	assert(tcgetattr(slave, &terminal) == 0 && !(terminal.c_lflag & ICANON));
	assert(PosixTerminalEnd() == 0 && !locked);
	assert(sigaction(SIGTERM, NULL, &actual) == 0);
	if (actual.sa_handler != SIG_DFL)
		fprintf(stderr, "one-shot flags: requested=%x queried=%x restored=%x; handled=%d\n",
		        original.sa_flags, one_shot_flags, actual.sa_flags, (int)handled);
	assert(actual.sa_handler == SIG_DFL);
	close(channel[0]); close(channel[1]); close(slave); close(master);
	check_background_cleanup();
	return 0;
}
