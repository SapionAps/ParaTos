#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "common.h"
#include "path.h"
#include "host.h"
#include "gemdos.h"
#include "arguments.h"


/**
 * allocate an environment and argument list for passing on to the next process
 */
static void prepare_args(char*** out_argv, char*** out_env, char** pathp, emuptr32_t cmdline, emuptr32_t env)
{
	char* path = *pathp;
	char** argv;
	char** envp;
	prepare_guest_args(&argv, &envp, path, cmdline, env);

	uint16_t magic;
	FILE* file = fopen(path,"r");
	if (file)
	{
		fread(&magic, sizeof(magic), 1, file);
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
	int32_t retval = TOS_E_OK;
	char* path = NULL;
	char* orig_path = NULL;
	if (arg1)
	{
		orig_path = path = read_path(arg1);
	}

	// mint uses the high bit to control tracing
	switch (mode & 0x7fff) {
		case 0: // load and go (basically fork/exec/wait)
		{
			struct sigaction new_act, int_act, quit_act;
			new_act.sa_handler = SIG_IGN;
			sigaction(SIGINT, &new_act, &int_act);
			sigaction(SIGQUIT, &new_act, &quit_act);
			pid_t pid = fork();
			if (pid == -1)
			{
				int ret = MapErrno();
				sigaction(SIGINT, &int_act, NULL);
				sigaction(SIGQUIT, &quit_act, NULL);
				return ret;
			}
			else if (pid)
			{
				int status;
				waitpid(pid, &status, 0);
				sigaction(SIGINT, &int_act, NULL);
				sigaction(SIGQUIT, &quit_act, NULL);
				if(WIFEXITED(status))
				{
					return WEXITSTATUS(status);
				}
				else
				{
					return -1;
				}
			}
			else
			{
				_exit(Pexec(200, arg1, arg2, arg3));
			}
		}
		break;
		case 200: // replace process
		{
			char** argv;
			char** envp;
			prepare_args(&argv, &envp, &path, arg2, arg3);
			execve(path, argv, envp);
			retval = MapErrno();
			perror(path);
		}
		default:
		NOT_IMPLEMENTED(GEMDOS, Pexec_mode, mode);
		retval = TOS_ENOSYS;
	}
	if(orig_path)
		free(orig_path);
	return retval;
}
