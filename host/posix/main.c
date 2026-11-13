#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "gemdos.h"
#include "loader.h"
#include "cookiejar.h"
#include "host.h"
#include "tasks.h"

extern char **environ;

int main(int argc, char* argv[])
{
	/* Parent-side virtual identity survives exec into a new emulator. These
	 * launcher arguments are consumed before building the guest command tail. */
	if (argc >= 5 && !strcmp(argv[1], "--paratos-child"))
	{
		char *end_id, *end_parent;
		long id = strtol(argv[2], &end_id, 10);
		long parent = strtol(argv[3], &end_parent, 10);
		if (*end_id || *end_parent || !id || id < -32768 || id > 32767 ||
			parent < -32768 || parent > 32767) return 1;
		PosixRootIdentity((int16_t)id, (int16_t)parent);
		argv[3] = argv[0];
		argv += 3;
		argc -= 3;
	}
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
