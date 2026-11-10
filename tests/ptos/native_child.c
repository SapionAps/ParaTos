/* Executed by native Pexec from emulated 68k code. The inherited file
 * handle, environment and working directory must belong to that caller. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mint/osbind.h>

int main(int argc, char **argv)
{
	char path[128] = {0};
	const char *value = getenv("DEMO");
	if (argc != 2 || !value || strcmp(value, "present"))
	{
		printf("native child argument/environment check: argc=%d\n", argc);
		return 1;
	}
	long directory = Dgetpath(path, 0);
	const char *relative = path[0] == '\\' || path[0] == '/' ? path + 1 : path;
	if (directory || strcmp(relative, "PTOSDEMO"))
	{
		printf("native child directory check: %s\n", path);
		return 2;
	}
	long written = Fwrite((short)atoi(argv[1]), 6, "native");
	if (written != 6) { printf("native child inherited write: %ld\n", written); return 3; }
	return 23;
}
