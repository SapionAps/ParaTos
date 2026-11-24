#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "arguments.h"

static void parse_cmdline(emuptr32_t cmdline, char** argv, char* buffer, int *arg_countp, int *buffer_sizep)
{
	int cmd_length = cmdline ? m68k_read_memory_8(cmdline) : 0;
	char *cmd = (char *)memory + cmdline + 1;
	if (cmd_length > 127) cmd_length = 127;
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
					if(i + 1 >= cmd_length || cmd[i+1] != state)
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


int prepare_guest_args(char ***out_argv, char ***out_env, char *path,
	emuptr32_t cmdline, emuptr32_t env, size_t env_bytes)
{
	/* Own the environment and both argv formats until execve consumes them. */
	int argc = 1, cmd_bytes = 0;
	parse_cmdline(cmdline, NULL, NULL, &argc, &cmd_bytes);

	const char *source = env ? (const char *)memory + env : "\0";
	size_t env_count = 0;
	for (const char *entry = source; *entry; entry += strlen(entry) + 1)
		env_count++;

	size_t pointer_count = env_count + argc + 3;
	char **envp = malloc(pointer_count * sizeof(*envp) + cmd_bytes + env_bytes);
	if (!envp) return -1;

	char *cmd = (char *)(envp + pointer_count);
	char *copy = cmd + cmd_bytes;
	memcpy(copy, source, env_bytes);
	for (size_t i = 0; i < env_count; i++)
	{
		envp[i] = copy;
		copy += strlen(copy) + 1;
	}
	envp[env_count] = NULL;

	char **argv = NULL;
	if (cmdline && m68k_read_memory_8(cmdline) == 127)
	{
		for (size_t i = 0; i < env_count; i++)
		{
			if (strcmp("ARGV", envp[i]) && strncmp("ARGV=", envp[i], 5)) continue;
			envp[i] = NULL;
			argv = envp + i + 1;
			break;
		}
	}
	if (!argv)
	{
		argv = envp + env_count + 1;
		argv[0] = path;
		parse_cmdline(cmdline, argv + 1, cmd, NULL, NULL);
	}
	*out_env = envp;
	*out_argv = argv;
	return 0;
}
