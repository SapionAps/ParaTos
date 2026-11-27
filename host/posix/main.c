#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "gemdos.h"
#include "loader.h"
#include "cookiejar.h"
#include "host.h"
#include "tasks.h"
#include "guest_memory.h"
#include "../../guest_launch.h"

extern char **environ;

int main(int argc, char* argv[])
{
	/* Parent-side virtual identity survives exec into a new emulator. */
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
	const char *program = argv[1];
	char tail[128], *environment = NULL;
	size_t environment_size = 0;
	if (!strcmp(program, "--guest-prg"))
	{
		int index = guest_decode_options(argc, argv, tail, &environment,
			POSIX_RAM_BYTES, &environment_size);
		if (index < 0)
		{
			fprintf(stderr, "Paratos: invalid guest launch options (GEMDOS error %d).\n", index);
			return 1;
		}
		program = argv[2];
		if (!environment)
			environment = guest_build_environment(argc - index, argv + index,
				argv[index], environ, POSIX_RAM_BYTES, &environment_size);
		if (!environment)
		{
			fprintf(stderr, "Paratos: cannot build guest environment.\n");
			return 1;
		}
	}

	FindExecutable();
	InitM68KMemory();
	InitMemory();
	InitCookieJar();

	raw_prg_arguments raw = {tail, environment, environment_size};
	current_process = LoadExe(program, argv + 1, argc - 1, environ, environment ? &raw : NULL);
	free(environment);
	if (current_process == 0)
	{
		fprintf(stderr, "error: cannot load %s.\n", program);
		return 1;
	}

	return PosixRunRoot(current_process);
}
