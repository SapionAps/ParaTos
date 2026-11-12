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
	FILE* file = fopen(path,"r");
	if (file)
	{
		if (fread(&magic, sizeof(magic), 1, file) != 1) magic = 0;
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
		fclose(file);
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
	char *path = original;
	char **argv, **envp;
	prepare_args(&argv, &envp, &path, arg2, arg3);
	if (operation == 200)
	{
		int saved[3] = {-1, -1, -1};
		for (int fd = 0; fd < 3; fd++)
		{
			saved[fd] = fcntl(fd, F_DUPFD_CLOEXEC, 6);
			if (saved[fd] < 0 && errno != EBADF)
			{
				int32_t result = MapErrno();
				for (int i = 0; i < fd; i++) if (saved[i] >= 0) close(saved[i]);
				free(envp);
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
		free(envp);
		free(original);
		return result;
	}
	pid_t pid = fork();
	if (!pid)
	{
		/* Only async-signal-safe operations after fork: other guest threads
		 * may have been active in the parent. */
		PosixExportStandardHandles();
		execve(path, argv, envp);
		_exit(127);
	}
	int32_t result = pid < 0 ? MapErrno() : 0;
	free(envp);
	free(original);
	if (pid < 0) return result;
	if (operation == 100) return PosixRegisterPid(pid);

	PosixPause();
	int status;
	pid_t waited;
	do { waited = waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
	int saved_errno = errno;
	PosixResume();
	errno = saved_errno;
	if (waited < 0) return MapErrno();
	return WIFEXITED(status) ? WEXITSTATUS(status) : TOS_ERROR;
}
