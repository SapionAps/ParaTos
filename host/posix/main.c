#include <stdio.h>

#include "common.h"
#include "gemdos.h"
#include "loader.h"
#include "cookiejar.h"
#include "host.h"
#include "tasks.h"

extern char **environ;

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <program.tos> [arguments...]\n", argv[0]);
		return 1;
	}

	FindExecutable();
	InitM68KMemory();
	InitMemory();
	InitCookieJar();

	current_process = LoadExe(argv[1], argv+1, argc-1, environ);
	if (current_process == 0)
	{
		fprintf(stderr, "error: cannot load %s.\n", argv[1]);
		return 1;
	}

	return PosixRunRoot(current_process);
}
