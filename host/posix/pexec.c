#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include "common.h"
#include "path.h"
#include "host.h"
#include "gemdos.h"
#include "arguments.h"
#include "tasks.h"


/**
 * allocate an environment and argument list for passing on to the next process
 */
static void prepare_args(char*** out_argv, char*** out_env, char** pathp, emuptr32_t cmdline, emuptr32_t env)
{
	char* path = *pathp;
	char** argv;
	char** envp;
	prepare_guest_args(&argv, &envp, path, cmdline, env);

	uint16_t magic = 0;
	/* Executable sniffing must not rendezvous with FIFO writers or read
	 * devices under the execution mutex. Inspect regular files only, using
	 * nonblocking open plus fstat to cover path replacement races. */
	struct stat info;
	if (!stat(path, &info) && S_ISREG(info.st_mode))
	{
		int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
		if (fd >= 0)
		{
			if (!fstat(fd, &info) && S_ISREG(info.st_mode))
			{
				ssize_t count;
				do { count = read(fd, &magic, sizeof(magic)); } while (count < 0 && errno == EINTR);
				if (count != sizeof(magic)) magic = 0;
			}
			close(fd);
		}
	}
	magic = paratos_be16toh(magic);
	if (magic == 0x601a) // this is another TOS executable - prepend paratos to the command line
	{
		// Shift argv one element to the right
		char* prev = argv[0];
		for(int i=1; true; i++)
		{
			char* next = argv[i];
			argv[i] = prev;
			if (prev == 0)
				break;
			prev = next;
		}

		// add paratos executable in front of the command line and change the path name to execute
		// TODO: handle when argv[0] != pathname
		*pathp = paratos_exe;
		argv[0] = paratos_exe;
	}

	*out_env = envp;
	*out_argv = argv;
}

/**
 * Pexec - 75
 *
 * The function Pexec loads and launches a program. With the parameter mode
 * one can select from several variants.
 *
 * int32_t Pexec ( uint16_t mode, ... )
 */
static char **tag_guest_argv(char **argv, int16_t id, int16_t parent,
	char id_text[16], char parent_text[16])
{
	size_t count = 0;
	while (argv[count]) count++;
	char **tagged = malloc((count + 4) * sizeof(*tagged));
	if (!tagged) return NULL;
	snprintf(id_text, 16, "%d", id);
	snprintf(parent_text, 16, "%d", parent);
	tagged[0] = argv[0];
	tagged[1] = "--paratos-child";
	tagged[2] = id_text;
	tagged[3] = parent_text;
	for (size_t i = 1; i <= count; i++) tagged[i + 3] = argv[i];
	return tagged;
}

int32_t Pexec ( uint16_t mode, emuptr32_t arg1, emuptr32_t arg2, emuptr32_t arg3 )
{
	uint16_t operation = mode & 0x7fff;
	if (operation == 4 || operation == 6 || operation == 104)
		return PosixGo(arg2, operation == 6, operation == 104);
	if (operation == 5) return PosixCreateBasepage(arg2, arg3);
	if (operation != 0 && operation != 3 && operation != 100 && operation != 200)
		return TOS_ENOSYS;
	if (!arg1 || arg1 >= memory_sz || !memchr(memory + arg1, 0, memory_sz - arg1))
		return TOS_EINVAL;
	char *original = read_path(arg1);
	if (!original) return TOS_EPTHNF;
	if (operation == 3)
	{
		int32_t result = PosixLoadImage(original, arg2, arg3);
		free(original);
		return result;
	}
	if ((arg2 && (arg2 >= memory_sz || memory_sz - arg2 < 128)) ||
		(arg3 && (arg3 >= memory_sz || memory_sz - arg3 < 2)))
	{
		free(original);
		return TOS_EINVAL;
	}
	if (!arg3) arg3 = m68k_read_field(current_process, basepage_t, p_env);
	if (PosixEnvironmentSize(arg3) < 0) { free(original); return TOS_EINVAL; }
	char *path = original;
	char **argv, **envp;
	prepare_args(&argv, &envp, &path, arg2, arg3);
	char **tagged = NULL;
	char id_text[16], parent_text[16];
	if (operation == 200)
	{
		if (path == paratos_exe)
		{
			tagged = tag_guest_argv(argv, PosixTaskPid(), PosixTaskParentPid(), id_text, parent_text);
			if (!tagged) { free(tagged); free(envp); free(original); return TOS_ENSMEM; }
			argv = tagged;
		}
		int saved[3] = {-1, -1, -1};
		for (int fd = 0; fd < 3; fd++)
		{
			saved[fd] = fcntl(fd, F_DUPFD_CLOEXEC, 6);
			if (saved[fd] < 0 && errno != EBADF)
			{
				int32_t result = MapErrno();
				for (int i = 0; i < fd; i++) if (saved[i] >= 0) close(saved[i]);
				free(tagged); free(envp);
				free(original);
				return result;
			}
		}
		PosixExportStandardHandles();
		execve(path, argv, envp);
		int32_t result = MapErrno();
		/* A failed replacement must not redirect other guest threads. */
		for (int fd = 0; fd < 3; fd++)
		{
			if (saved[fd] < 0) close(fd);
			else { dup2(saved[fd], fd); close(saved[fd]); }
		}
		free(tagged); free(envp);
		free(original);
		return result;
	}
	/* Reserve identity and ownership before a child can exist. Synchronous
	 * launches remain invisible to every guest's Pwait until their own join. */
	int16_t id = PosixReservePid(1);
	if (id < 0) { free(tagged); free(envp); free(original); return TOS_ENHNDL; }
	if (path == paratos_exe)
	{
		tagged = tag_guest_argv(argv, id, PosixTaskPid(), id_text, parent_text);
		if (!tagged) { PosixReleasePid(id); free(tagged); free(envp); free(original); return TOS_ENSMEM; }
		argv = tagged;
	}
	int channel[2];
	if (pipe(channel) < 0)
	{
		int32_t error = MapErrno();
		PosixReleasePid(id); free(tagged); free(envp); free(original);
		return error;
	}
	for (int i = 0; i < 2; i++)
	{
		int native = channel[i];
		channel[i] = PosixAdoptFd(native);
		if (channel[i] < 0 || fcntl(channel[i], F_SETFD, FD_CLOEXEC) < 0)
		{
			int32_t error = MapErrno();
			if (channel[0] >= 0) close(channel[0]);
			if (channel[1] >= 0) close(channel[1]);
			PosixReleasePid(id); free(tagged); free(envp); free(original);
			return error;
		}
	}
	pid_t pid = fork();
	if (!pid)
	{
		/* Only async-signal-safe operations in the forked child. */
		close(channel[0]);
		PosixExportStandardHandles();
		execve(path, argv, envp);
		int error = errno;
		const char *bytes = (const char *)&error;
		size_t sent = 0;
		while (sent < sizeof(error))
		{
			ssize_t n = write(channel[1], bytes + sent, sizeof(error) - sent);
			if (n < 0 && errno == EINTR) continue;
			if (n <= 0) break;
			sent += (size_t)n;
		}
		_exit(127);
	}
	int32_t result = pid < 0 ? MapErrno() : 0;
	free(tagged); free(envp); free(original);
	close(channel[1]);
	if (pid < 0)
	{
		close(channel[0]); PosixReleasePid(id);
		return result;
	}
	PosixBindPid(id, pid, 1);
	PosixPause();
	int launch_error = 0;
	size_t received = 0;
	while (received < sizeof(launch_error))
	{
		ssize_t n = read(channel[0], (char *)&launch_error + received, sizeof(launch_error) - received);
		if (n < 0 && errno == EINTR) continue;
		if (n < 0) { launch_error = errno; received = sizeof(launch_error); kill(pid, SIGKILL); break; }
		if (!n) break;
		received += (size_t)n;
	}
	close(channel[0]);
	if (received && received != sizeof(launch_error)) { launch_error = EIO; kill(pid, SIGKILL); }
	if (!received && operation == 100)
	{
		PosixResume();
		PosixBindPid(id, pid, 0);
		return id;
	}
	int status;
	pid_t waited;
	do { waited = waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
	int saved_errno = launch_error ? launch_error : errno;
	PosixResume();
	PosixReleasePid(id);
	errno = saved_errno;
	if (launch_error || waited < 0) return MapErrno();
	return WIFEXITED(status) ? WEXITSTATUS(status) : TOS_ERROR;
}
