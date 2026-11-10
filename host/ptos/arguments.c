#include <ctype.h>
#include <string.h>
#include "host.h"

void ptos_command_tail(char tail[128], int argc, char **argv)
{
	int quoted = 0, extended = 0;
	for (int i = 0; i < argc; i++)
	{
		if (!*argv[i]) { quoted = 1; extended = 1; }
		for (const char *s = argv[i]; *s; s++)
			if (isspace((unsigned char)*s) || *s == '\'') quoted = 1;
	}
	/* MagX convention: quote every argument if quoting is needed, and
	 * double embedded single quotes. ARGV remains the lossless source for
	 * empty arguments and command lines longer than the classic tail. */
	memset(tail, 0, 128);
	size_t n = 0;
#define APPEND(ch) do { if (n < 126) tail[1 + n] = (ch); n++; } while (0)
	for (int i = 0; i < argc; i++)
	{
		if (i) APPEND(' ');
		if (quoted) APPEND('\'');
		for (const char *s = argv[i]; *s; s++)
		{
			APPEND(*s);
			if (quoted && *s == '\'') APPEND('\'');
		}
		if (quoted) APPEND('\'');
	}
#undef APPEND
	tail[0] = extended || n > 126 ? 127 : (char)n;
}
