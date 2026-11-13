#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <termios.h>
#include <unistd.h>
#include <sys/wait.h>

#include "tasks.h"

/* The terminal mutex serializes sessions. This lock also protects the saved
 * session from signal handlers running on other host threads. Normal callers
 * block these signals before taking it; handlers never acquire runtime locks. */
static atomic_flag cleanup_lock = ATOMIC_FLAG_INIT;
static const int signals[] = { SIGHUP, SIGINT, SIGQUIT, SIGTERM, SIGABRT };
static struct sigaction previous[sizeof(signals) / sizeof(signals[0])];
static int replaced[sizeof(signals) / sizeof(signals[0])];
static size_t installed;
static int terminal_fd = -1, raw_active;
static struct termios saved;
static int guardian_channel = -1;
static pid_t guardian_pid = -1;

/* Darwin cannot report SA_RESETHAND through sigaction's query. Leave custom
 * actions entirely native and use an exit observer for these uncommon sessions:
 * if a one-shot action resets to default, process death still restores the tty.
 * The post-fork child uses only async-signal-safe operations. */
static int start_guardian(void)
{
	long descriptor_limit = sysconf(_SC_OPEN_MAX);
	if (descriptor_limit < 0) return -1;
	int channel[2];
	if (pipe(channel) < 0) return -1;
	for (int i = 0; i < 2; i++)
	{
		int result;
		if (channel[i] < 3)
		{
			result = fcntl(channel[i], F_DUPFD_CLOEXEC, 3);
			if (result >= 0) { close(channel[i]); channel[i] = result; }
		}
		else result = fcntl(channel[i], F_SETFD, FD_CLOEXEC);
		if (result < 0)
		{
			int error = errno;
			close(channel[0]); close(channel[1]); errno = error;
			return -1;
		}
	}
	pid_t pid = fork();
	if (!pid)
	{
		/* This observer must neither run application callbacks nor stop on
		 * catchable job-control signals sent to the reader's process group. */
		sigset_t blocked;
		sigfillset(&blocked);
		sigprocmask(SIG_SETMASK, &blocked, NULL);
		/* Do not keep unrelated guest pipes or files alive while observing
		 * this session. The limit is obtained before fork, outside the child. */
		for (long fd = 0; fd < descriptor_limit; fd++)
			if (fd != channel[0] && fd != terminal_fd) close((int)fd);
		char command;
		ssize_t count;
		do { count = read(channel[0], &command, 1); } while (count < 0 && errno == EINTR);
		if (count != 1)
		{
			int result;
			do { result = tcsetattr(terminal_fd, TCSANOW, &saved); } while (result < 0 && errno == EINTR);
		}
		_exit(0);
	}
	int error = errno;
	close(channel[0]);
	if (pid < 0) { close(channel[1]); errno = error; return -1; }
	guardian_channel = channel[1];
	guardian_pid = pid;
	return 0;
}

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
		if (replaced[i]) sigaction(signals[i], &previous[i], NULL);
	}
	if (guardian_channel >= 0)
	{
		/* Only disarm after successful restoration. EOF otherwise lets the
		 * guardian retry, including after a failed setup or termination. */
		if (!result)
		{
			ssize_t count;
			do { count = write(guardian_channel, "D", 1); } while (count < 0 && errno == EINTR);
		}
		close(guardian_channel);
		guardian_channel = -1;
	}
	if (result < 0) errno = error;
	return result;
}

static void terminate_session(int signal_number)
{
	int error = errno;
	lock_cleanup();
	restore_session();
	atomic_flag_clear_explicit(&cleanup_lock, memory_order_release);
	/* Only default dispositions are wrapped. Re-delivery retains default
	 * termination status and core-dump semantics. User handlers stay installed
	 * with their original siginfo, context, masks, and one-shot behavior. */
	sigset_t signal_set;
	sigemptyset(&signal_set);
	sigaddset(&signal_set, signal_number);
	sigprocmask(SIG_UNBLOCK, &signal_set, NULL);
	raise(signal_number);
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
	action.sa_handler = terminate_session;
	action.sa_mask = set;
	int custom = 0;
	for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++)
	{
		if (sigaction(signals[i], NULL, &previous[i]) < 0) goto out;
		/* Preserve user/ignored actions in the kernel. In particular Darwin's
		 * sigaction query omits SA_RESETHAND, so saving/restoring such an action
		 * would silently lose its one-shot semantics. Returning handlers leave
		 * the active reader in its protected raw session. */
		replaced[i] = previous[i].sa_handler == SIG_DFL;
		if (!replaced[i] && previous[i].sa_handler != SIG_IGN) custom = 1;
		if (replaced[i] && sigaction(signals[i], &action, NULL) < 0) goto out;
		installed++;
	}
	if (custom && start_guardian() < 0) goto out;
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
	pid_t reap = result < 0 ? guardian_pid : -1;
	if (result < 0) guardian_pid = -1;
	atomic_flag_clear_explicit(&cleanup_lock, memory_order_release);
	pthread_sigmask(SIG_SETMASK, &old, NULL);
	if (result < 0) PosixTerminalUnlock();
	if (reap > 0)
	{
		while (waitpid(reap, NULL, 0) < 0 && errno == EINTR) {}
	}
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
	pid_t reap = guardian_pid;
	guardian_pid = -1;
	atomic_flag_clear_explicit(&cleanup_lock, memory_order_release);
	pthread_sigmask(SIG_SETMASK, &old, NULL);
	PosixTerminalUnlock();
	if (reap > 0)
	{
		while (waitpid(reap, NULL, 0) < 0 && errno == EINTR) {}
	}
	errno = error;
	return result;
}
