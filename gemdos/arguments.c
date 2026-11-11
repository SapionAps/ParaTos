#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "arguments.h"

/**
 * reads a list of zero terminated strings, terminated by an empty string at the end, and allocates an array of strng pointers
 * the result must be released using free
 */
static char** read_env(emuptr32_t address, size_t* out_size)
{
	size_t array_count=0;
	size_t current_strsz=0;
	size_t src_chars = 0;

	char* src = (char*)memory+address;

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
	char * cmd = (char*)memory+cmdline+1;
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


void prepare_guest_args(char*** out_argv, char*** out_env, char* path, emuptr32_t cmdline, emuptr32_t env)
{
	size_t env_sz;

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
	}

	*out_env = envp;
	*out_argv = argv;
}
