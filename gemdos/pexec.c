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
#include "tos_errors.h"
#include "gemdos.h"

extern char* paratos_exe;

/**
 * reads a list of zero terminated strings, terminated by an empty string at the end, and allocates an array of strng pointers
 * the result must be released using free
 */
static char** read_env(emuptr32_t address, size_t* out_size)
{
	size_t array_count=0;
	size_t current_strsz=0;
	size_t src_chars = 0;

	char* src = memory+address;

	do {
		char c = src[src_chars++];
		if (c == 0)
		{
			if(current_strsz == 0)
			{
				break;
			}
			else
			{
				array_count++;
				current_strsz=0;
			}
		}
		else
		{
			current_strsz++;
		}
	} while(true);

	// note we allocate one extra entry in case we need extra room for the paratos executable name
	size_t ptr_arraysz = (array_count+2) * sizeof(char*);
	char** result = malloc(ptr_arraysz);

	char* current = src;
	int j=0;
	for(int i=0; i<src_chars; i++)
	{
		if(src[i]==0)
		{
			result[j++]=current;
			current=src+i+1;
		}
	}
	// Add a terminiating null ptr
	result[array_count]=NULL;

	if(out_size)
	{
		*out_size = array_count;
	}

	return result;
}

static void parse_cmdline(emuptr32_t cmdline, char** argv, char* buffer, int *arg_countp, int *buffer_sizep)
{
	int cmd_length = m68k_read_memory_8(cmdline);
	char * cmd = memory+cmdline+1;
	int byte_count=0;
	enum { BL, CHAR, DQ='"', SQ='\'' } state = BL;
	for(int i=0; i<cmd_length && cmd[i]; i++)
	{
		switch (state)
		{
			case BL:
			{
				if (strchr(" \t\n\r", cmd[i]) == NULL)
				{
					if(argv && buffer)
						*(argv++) = buffer;
					if(arg_countp)
						(*arg_countp)++;
					state = CHAR;
					i--; // backup one char
				}
			}
			break;
			case CHAR:
			{
				switch (cmd[i])
				{
					case ' ':
					case '\t':
					case '\n':
					case '\r':
						if(buffer)
							*buffer++ = 0;
						printf("BUFFER[%d]=0\n",byte_count);
						byte_count++;
						state = BL;
						break;
					case '"':
						state = DQ;
						break;
					case '\'':
						state = SQ;
						break;
					default:
						if(buffer)
							*buffer++ = cmd[i];
						printf("BUFFER[%d]=%c\n",byte_count, cmd[i]);
						byte_count++;
				}
			}
			break;
			case DQ:
			case SQ:
			{
				if (cmd[i] == state)
				{
					if(cmd[i+1] != state)
					{
						state = CHAR;
					}
					else
					{
						if(buffer)
							*buffer++ = state;
						printf("BUFFER[%d]=%c\n",byte_count, state);
						byte_count++;
						i+=2;
					}
				}
			}
			break;
		}
	}
	if(buffer)
		*buffer++ = 0;
	byte_count++;

	if(argv)
		*(argv++) = NULL;

	if(buffer_sizep)
		*buffer_sizep = byte_count;
}


/**
 * allocate an environment and argument list for passing on to the next process
 */
static void prepare_args(char*** out_argv, char*** out_env, char** pathp, emuptr32_t cmdline, emuptr32_t env)
{
	size_t env_sz;
	char* path = *pathp;

	char** argv = NULL;
	char** envp = read_env(env, &env_sz);

	int cmd_length = m68k_read_memory_8(cmdline); // cmdline is a pascal string
	if(cmd_length == 127) // ARGV procedure: real command line is at the end of the env string.
	{
		for(int i=0; envp[i]; i++)
		{
			if(strcmp("ARGV", envp[i]) == 0 || strncmp("ARGV=", envp[i], 5) == 0)
			{
				argv = envp+(i+1);
				envp[i]=NULL;
				break;
			}
		}
	}

	// parse old-style fixed size command line
	if (argv==NULL)
	{
		// first figure out how many entries we need to allocate in argv
		int arg_count = 1;
		int byte_count = 0;
		parse_cmdline(cmdline, NULL, NULL, &arg_count, &byte_count);

		// Place everything into the same buffer
		size_t new_env_size = (env_sz+arg_count+3) * sizeof(char*) + byte_count;
		envp = realloc(envp, new_env_size);
		argv = &envp[env_sz+1];

		// The actual command line bytes are placed after the argv pointers
		char* buffer = (char*)&argv[arg_count+2];

		// First argument is the executable itself, as the TOS cmdline only contains argv[1] and forward
		argv[0]=path;
		parse_cmdline(cmdline, argv+1, buffer, NULL, NULL);
		for(int i=0; argv[i]; i++)
		{
			printf("ARG[%d]: %s\n", i, argv[i]);
		}
	}

	uint16_t magic;
	FILE* file = fopen(path,"r");
	if (file)
	{
		fread(&magic, sizeof(magic), 1, file);
		magic = be16toh(magic);
		printf("Magic: %x\n", magic);
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
	char* buffer=alloca(1024);
	if (arg1)
	{
		m68k_read_string(arg1, buffer, 1023, 1);
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
			prepare_args(&argv, &envp, &buffer, arg2, arg3);
			execve(buffer, argv, envp);
			perror(buffer);
			return MapErrno();
		}
		default:
		NOT_IMPLEMENTED(GEMDOS, Pexec_mode, mode);
		return TOS_ENOSYS;
	}
}
