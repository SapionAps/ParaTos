#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "common.h"
#include "gemdos.h"
#include "path.h"

char* read_path(emuptr32_t tos_path)
{
	return tos_path_to_unix(&memory[tos_path]);
}

static int next_path_sep(char* path, int start)
{
	while(!END_OF_NAME(path[start]))
	{
		start++;
	}
	return start;
}

// Helper to append to the result string and resizing the buffer when required
#define append(src, offset, len) ({ \
	const char* s = (src); \
	int o = (offset); \
	int l = (len); \
	bool reallocate = false; \
	while (result_sz < o + l + 1) \
	{\
		result_sz *= 2; \
		reallocate = true; \
	}\
	if (reallocate) \
	{ \
		result = realloc(result, result_sz); \
		if (!result) \
			return NULL; /* out of memory */ \
	} \
	memcpy(&result[o], s, l); \
	result[o+l] = '\0'; \
	result_len = o+l; \
})

char* tos_path_to_unix(char* tos_path)
{
	size_t tos_len = strlen(tos_path);
	if(tos_len >= 2 && tos_path[1] == ':')
	{
		// Todo: support multiple drive mappings
		if (tos_path[0] == 'u' || tos_path[0] == 'U')
		{
			tos_path+=2;
			tos_len-=2;
		}
		else
		{
			return NULL;
		}
	}
	size_t result_sz = tos_len * 2;
	size_t result_len = 0;
	char* result = malloc(result_sz);
	if (PATH_SEP(tos_path[0]))
	{
		append("/", 0, 1);
	}

	struct stat st;
	char short_name[13];
	for(int i=0, j=next_path_sep(tos_path, 0) ; tos_path[i]; i=j+1, j=next_path_sep(tos_path, j+1))
	{
		size_t current = result_len;

		if(i==j)
			continue;
		append(&tos_path[i], current, j-i);
		if (lstat(result, &st) == -1)
		{
			// Attempt scanning the parent directory for the curent component
			char* curname = &result[current];
			const char* curdir = ".";
			if (current)
			{
				result[current-1]=0;
				curdir = current==1?"/":result;
			}
			DIR* dir = opendir(curdir);
			if (!dir)
			{
				free(result);
				return NULL;
			}

			bool found = false;
			struct dirent* entry;
			while(entry = readdir(dir))
			{
				if(strcasecmp(curname, entry->d_name) == 0)
				{
					goto match;
				}
				if(j-i <= 12)
				{
					filename8_3(short_name, entry->d_name);
					if(strcasecmp(curname, short_name) == 0)
					{
						goto match;
					}
				}
				continue;
match:
				found = true;
				if(current)
				{
					result[current-1]='/';
				}
				append(entry->d_name, current, strlen(entry->d_name));
				break;
			}
			closedir(dir);
			if (!found)
			{
				if(!tos_path[j]) // if it's the last path component simply return the unmodified path name
				{
					if(current)
					{
						result[current-1]='/';
					}
				}
				else
				{
					free(result);
					return NULL;
				}
			}
		}
		if(!tos_path[j])
		{
			break;
		}
		append("/", result_len, 1);
	}

	return result;
}

#undef append
