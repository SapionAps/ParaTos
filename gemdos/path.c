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

const char* filename8_3(char* dest, const char* source)
{
	bool needsHash = false;
	bool uppercase = !m68k_read_field(current_process, basepage_t, mint_domain);
	int slen = 0; const char* retval;
	for(slen=0;!END_OF_NAME(source[slen]);slen++);
	retval = source + slen;

	char* d=dest;
	if (source[0] == '.' && ( slen == 1 || slen == 2 && source[1] == '.'))
	{
		// special case for . and ..
		for(int i = 0; i<slen;i++)
		{
			*d++='.';
		}
		*d++=0;
		return retval;
	}

	// Find last dot in filename
	const char* dot=0;
	for (const char* c=source+slen-1; c>=source; c--)
	{
		if (*c == '.')
		{
			dot=c;
			break;
		}
	}

	// If last dot is at the front of the filename, treat as no extension
	const char* s=source;
	if (dot == source)
	{
		dot=0;
	}

	// If any parts of the file name don't fit withing the 8.3 limits, we'll need to mangle the filename
	if(slen > 12 || dot && ((dot-source) > 8 || (slen - (dot-source)) > 4) ||!dot && slen > 8)
	{
		needsHash=true;
	}

	for(int i=0; i<8; i++)
	{
		if(END_OF_NAME(*s) || (dot && s==dot))
		{
			// make room for the hash
			if(needsHash)
			{
				for(int j=i; j<8; j++)
					*d++='~';
			}
			break;
		}
		if( *s == '.')
		{
			*d++='_'; s++;
			needsHash=true;
		}
		else
		{
			*d++=uppercase?toupper(*s++):*s++;
		}
	}

	if(dot)
	{
		*d++='.';
		s=dot+1;
		for(int i=0; i<3; i++)
		{
			if(END_OF_NAME(*s))
				break;
			*d++=uppercase?toupper(*s++):*s++;
		}
	}
	*d=0;

	if(needsHash)
	{
		static const char hash_alphabet[32] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";
		uint16_t hash=0xf00f;
		for (s=source; !END_OF_NAME(s[1]); s++)
			hash = (hash << 3) ^ (hash >> 5) ^ *s ^ (s[1] << 8);
		hash = (hash << 3) ^ (hash >> 5) ^ *s;
		dest[4]='~';
		dest[5]=hash_alphabet[(hash>>10)&0x1f];
		dest[6]=hash_alphabet[(hash>>5)&0x1f];
		dest[7]=hash_alphabet[hash&0x1f];
	}
	return retval;
}

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
