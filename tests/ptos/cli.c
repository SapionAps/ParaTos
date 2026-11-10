/* A real 68k CLI program; link with libcmini for the integration check in
 * host/ptos/README.txt. No Paratos-specific guest API is used. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mint/basepage.h>

int main(int argc, char **argv)
{
	if (argc != 3 || strcmp(argv[1], "two words") || strcmp(argv[2], "")) return 1;
	if ((unsigned char)_base->p_cmdlin[0] != 127 || strcmp(_base->p_cmdlin + 1, "'two words' ''")) return 8;
	const char *env = getenv("DEMO");
	if (!env || strcmp(env, "present")) return 2;
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
