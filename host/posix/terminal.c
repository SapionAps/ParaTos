#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <termios.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/ucontext.h>
#else
#include <ucontext.h>
#endif

#include "tasks.h"

/* The terminal mutex serializes sessions. This lock also protects the saved
 * session from signal handlers running on other host threads. Normal callers
 * block these signals before taking it; handlers never acquire runtime locks. */
static atomic_flag cleanup_lock = ATOMIC_FLAG_INIT;
static const int signals[] = { SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGABRT };
static struct sigaction previous[sizeof(signals) / sizeof(signals[0])];
static atomic_int reset_handler[sizeof(signals) / sizeof(signals[0])];
static size_t installed;
static int terminal_fd = -1, raw_active;
static struct termios saved;

static void lock_cleanup(void)
{
	while (atomic_flag_test_and_set_explicit(&cleanup_lock, memory_order_acquire)) {}
}

static int restore_session(void)
{
	int result = 0;
	int error = 0;
	if (raw_active)
	{
		do { result = tcsetattr(terminal_fd, TCSANOW, &saved); } while (result < 0 && errno == EINTR);
		if (result < 0) error = errno;
		raw_active = 0;
	}
	while (installed)
	{
		size_t i = --installed;
		struct sigaction action = previous[i];
		if (atomic_load_explicit(&reset_handler[i], memory_order_relaxed))
		{
			action.sa_handler = SIG_DFL;
			action.sa_flags = 0;
		}
		sigaction(signals[i], &action, NULL);
	}
	if (result < 0) errno = error;
	return result;
}

static void terminate_session(int signal_number, siginfo_t *info, void *context)
{
	int error = errno;
	lock_cleanup();
	size_t i = 0;
	while (signals[i] != signal_number) i++;
	struct sigaction original = previous[i];
	if (atomic_load_explicit(&reset_handler[i], memory_order_relaxed))
	{
		original.sa_handler = SIG_DFL;
		original.sa_flags = 0;
	}
	if (original.sa_handler == SIG_DFL) restore_session();
	else if (original.sa_flags & SA_RESETHAND)
	{
		/* Keep our wrapper installed, but make subsequent delivery and normal
		 * session cleanup observe the user's one-shot disposition. */
		atomic_store_explicit(&reset_handler[i], 1, memory_order_relaxed);
	}
	atomic_flag_clear_explicit(&cleanup_lock, memory_order_release);
	if (original.sa_handler == SIG_DFL)
		/* Only default termination is re-delivered. User handlers receive the
		 * original siginfo/context, and returning handlers leave the active
		 * reader protected by its raw session even on another host thread. */
		raise(signal_number);
	else if (original.sa_handler != SIG_IGN)
	{
		sigset_t mask = ((ucontext_t *)context)->uc_sigmask, wrapper_mask;
		for (int sig = 1; sig < NSIG; sig++)
			if (sigismember(&original.sa_mask, sig) == 1) sigaddset(&mask, sig);
		if (!(original.sa_flags & SA_NODEFER)) sigaddset(&mask, signal_number);
		sigprocmask(SIG_SETMASK, &mask, &wrapper_mask);
		if (original.sa_flags & SA_SIGINFO) original.sa_sigaction(signal_number, info, context);
		else original.sa_handler(signal_number);
		sigprocmask(SIG_SETMASK, &wrapper_mask, NULL);
	}
	errno = error;
}

static void termination_set(sigset_t *set)
{
	sigemptyset(set);
	for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
		sigaddset(set, signals[i]);
	/* tcsetattr must also work after the process group is backgrounded. */
	sigaddset(set, SIGTTOU);
}

int PosixTerminalBegin(int fd)
{
	struct termios probe;
	/* Redirected input must not wait behind another guest's terminal/read. */
	if (tcgetattr(fd, &probe) < 0) return errno == ENOTTY ? 0 : -1;
	PosixTerminalLock();
	sigset_t set, old;
	termination_set(&set);
	int error = pthread_sigmask(SIG_BLOCK, &set, &old);
	if (error) { PosixTerminalUnlock(); errno = error; return -1; }
	lock_cleanup();
	int result = -1;
	terminal_fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
	if (terminal_fd < 0 || tcgetattr(terminal_fd, &saved) < 0) goto out;
	struct sigaction action = {0};
	action.sa_sigaction = terminate_session;
	for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
	{
		if (sigaction(signals[i], NULL, &previous[i]) < 0) goto out;
		atomic_store_explicit(&reset_handler[i], 0, memory_order_relaxed);
		action.sa_flags = SA_SIGINFO | (previous[i].sa_flags & (SA_RESTART | SA_ONSTACK));
		action.sa_mask = set;
		for (int sig = 1; sig < NSIG; sig++)
			if (sigismember(&previous[i].sa_mask, sig) == 1) sigaddset(&action.sa_mask, sig);
		/* An ignored signal must remain ignored throughout the session. */
		if (previous[i].sa_handler != SIG_IGN && sigaction(signals[i], &action, NULL) < 0) goto out;
		installed++;
	}
	struct termios raw = saved;
	cfmakeraw(&raw);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;
	if (tcsetattr(terminal_fd, TCSANOW, &raw) < 0) goto out;
	raw_active = 1;
	result = 1;
out:
	error = errno;
	if (result < 0)
	{
		restore_session();
		if (terminal_fd >= 0) close(terminal_fd);
		terminal_fd = -1;
	}
	atomic_flag_clear_explicit(&cleanup_lock, memory_order_release);
	pthread_sigmask(SIG_SETMASK, &old, NULL);
	if (result < 0) PosixTerminalUnlock();
	errno = error;
	return result;
}

int PosixTerminalEnd(void)
{
	sigset_t set, old;
	termination_set(&set);
	pthread_sigmask(SIG_BLOCK, &set, &old);
	lock_cleanup();
	int result = restore_session();
	int error = errno;
	close(terminal_fd);
	terminal_fd = -1;
	atomic_flag_clear_explicit(&cleanup_lock, memory_order_release);
	pthread_sigmask(SIG_SETMASK, &old, NULL);
	PosixTerminalUnlock();
	errno = error;
	return result;
}
