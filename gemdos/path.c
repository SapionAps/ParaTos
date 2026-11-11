#include <ctype.h>

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

