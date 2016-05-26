#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "common.h"
#include "tos_errors.h"
#include "gemdos.h"


/**
 * reads a list of zero terminated strings, terminated by an empty string at the end, and allocates an array of strng pointers
 * the result must be released using free_string_aray
 */
char** read_string_array(emuptr32_t array, size_t* out_size)
{
	size_t array_count=0;
	size_t current_strsz=0;
	size_t bytes_to_copy = 0;

	do {
		char c = m68k_read_memory_8(array+(bytes_to_copy++));
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

	size_t ptr_arraysz = (array_count+1) * sizeof(char*);
	char* buffer = malloc(ptr_arraysz + bytes_to_copy);
	char** result = (char**)buffer;
	buffer+=ptr_arraysz;

	char* current = buffer;
	int j=0;
	for(int i=0; i<bytes_to_copy; i++)
	{
		buffer[i]=m68k_read_memory_8(array+i);
		if(buffer[i]==0)
		{
			result[j++]=current;
			current=buffer+i+1;
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

/**
 * allocate an environment and argument list for passing on to the next process
 */
void prepare_args(char*** out_argv, char*** out_env, char* path, emuptr32_t cmdline, emuptr32_t env)
{
	size_t env_sz;
	char** argv = NULL;
	char** envp = read_string_array(env, &env_sz);
	*out_env = envp;
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
	if (argv==NULL)
	{
		argv = malloc(2*sizeof(char*));
		argv[0]=path;
		argv[1]=NULL;
	}
	*out_argv=argv;
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
	char buffer[1024];
	if (arg1)
	{
		m68k_read_string(arg1, buffer, 1023, 1);
	}

	switch (mode) {
		case 200: // replace process
		{
		char** argv;
		char** envp;
		prepare_args(&argv, &envp, buffer, arg2, arg3);
		execve(buffer, argv, envp);
		return MapErrno();
		}
		default:
		NOT_IMPLEMENTED(GEMDOS, Pexec_mode, mode);
		return TOS_ENOSYS;
	}
}
