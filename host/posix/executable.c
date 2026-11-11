#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "common.h"
#include "host.h"

char* paratos_exe;

void FindExecutable(void)
{
	size_t pathSize = 1024;
	while (true)
	{
		paratos_exe = malloc(pathSize);
		if (paratos_exe == NULL)
		{
			fprintf(stderr, "malloc: Out of memory.\n");
			exit(1);
		}
#ifdef __APPLE__
		uint32_t nativeSize = (uint32_t)pathSize;
		if (_NSGetExecutablePath(paratos_exe, &nativeSize) == 0)
			break;
		free(paratos_exe);
		pathSize = nativeSize;
#else
		ssize_t length = readlink("/proc/self/exe", paratos_exe, pathSize - 1);
		if (length == -1)
		{
			perror("readlink(\"/proc/self/exe\")");
			exit(1);
		}
		if ((size_t)length < pathSize - 1)
		{
			paratos_exe[length] = '\0';
			break;
		}
		free(paratos_exe);
		pathSize *= 2;
#endif
	}
}
