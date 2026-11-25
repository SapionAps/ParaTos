/* A real 68k CLI program; link with libcmini for the integration check in
 * host/ptos/README.txt. No Paratos-specific guest API is used. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mint/basepage.h>

int main(int argc, char **argv)
{
	const char *env = getenv("DEMO");
	if (env && !strcmp(env, "rawshort"))
	{
		char expected[128] = "\014plain  again";
		expected[127] = 0x5a;
		return argc == 3 && !strcmp(argv[1], "plain") && !strcmp(argv[2], "again") &&
			!memcmp(_base->p_cmdlin, expected, sizeof(expected)) ? 39 : 8;
	}
	if (env && !strcmp(env, "rawextended"))
	{
		char expected[128] = "\177'raw  display'\rpadding";
		expected[127] = 0x5a;
		return argc == 3 && !strcmp(argv[0], "original guest name") &&
			!strcmp(argv[1], "two words") && !*argv[2] &&
			!memcmp(_base->p_cmdlin, expected, sizeof(expected)) ? 40 : 8;
	}
	if (!env || strcmp(env, "present")) return 2;
	const char *expected_name = getenv("CHECKARGV0");
	if (expected_name && strcmp(argv[0], expected_name)) return 9;
	if (argc == 1) return _base->p_cmdlin[0] || _base->p_cmdlin[1] ? 8 : 38;
	if (argc == 2 && !strcmp(argv[1], "plain"))
		return _base->p_cmdlin[0] != 5 || strcmp(_base->p_cmdlin + 1, "plain") ? 8 : 38;
	if (argc != 3 || strcmp(argv[1], "two words") || strcmp(argv[2], "")) return 1;
	if (((unsigned char)_base->p_cmdlin[0] != 127 && _base->p_cmdlin[0] != 14) ||
		strncmp(_base->p_cmdlin + 1, "'two words' ''", 14)) return 8;
	FILE *file = fopen("cli-data.tmp", "wb");
	if (!file) return 3;
	if (fwrite("guest data", 1, 10, file) != 10 || fclose(file)) return 4;
	file = fopen("cli-data.tmp", "rb");
	if (!file) return 5;
	char bytes[11] = {0};
	if (fread(bytes, 1, 10, file) != 10 || fclose(file) || strcmp(bytes, "guest data")) return 6;
	if (remove("cli-data.tmp")) return 7;
	puts("CLI fixture passed");
	return 37;
}
