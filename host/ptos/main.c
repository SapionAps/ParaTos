#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "memory.h"
#include "guest.h"

extern char **environ;

static char *guest_environment(int argc, char **argv)
{
	size_t size = 64; /* ARGV=NULL: indices, terminators */
	for (char **e = environ; *e; e++) size += strlen(*e) + 1;
	for (int i = 1; i < argc; i++) size += strlen(argv[i]) + 16;
	if (size > 32766) return NULL;
	char *env = calloc(1, size), *out = env;
	if (!env) return NULL;
	for (char **e = environ; *e; e++)
	{
		size_t n = strlen(*e) + 1;
		memcpy(out, *e, n); out += n;
	}
	memcpy(out, "ARGV=", 5); out += 5;
	int first_empty = 1;
	for (int i = 1; i < argc; i++)
	{
		if (*argv[i]) continue;
		out += sprintf(out, first_empty ? "NULL:%d" : ",%d", i - 1);
		first_empty = 0;
	}
	*out++ = 0;
	for (int i = 1; i < argc; i++)
	{
		const char *arg = *argv[i] ? argv[i] : " ";
		size_t n = strlen(arg) + 1;
		memcpy(out, arg, n); out += n;
	}
	*out = 0;
	return env;
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: paratos.prg <program.prg> [arguments...]\n");
		return 1;
	}
	if ((uintptr_t)&main < PTOS_NATIVE_BASE)
	{
		fprintf(stderr, "Paratos: pTOS relocated the native image below 0x41000000; high-image loader support is required.\n");
		return 1;
	}
	int32_t result = ptos_memory_init();
	if (result < 0)
	{
		fprintf(stderr, "Paratos: cannot initialize guest RAM (GEMDOS error %ld); Ssystem reads and low fixed-mapping kernel support are required.\n", (long)result);
		return 1;
	}
	ptos_allocator_init();
	char *env = guest_environment(argc, argv);
	if (!env) { result = PTOS_ERANGE; goto done; }
	char tail[128];
	ptos_command_tail(tail, argc - 2, argv + 2);
	char filename[1024];
	if (strlen(argv[1]) >= sizeof(filename)) { result = PTOS_ERANGE; free(env); goto done; }
	for (size_t i = 0; i <= strlen(argv[1]); i++) filename[i] = argv[1][i] == '/' ? '\\' : argv[1][i];
	result = ptos_load(filename, tail, env);
	free(env);
	if (result > 0) result = ptos_run_program(result);
	else fprintf(stderr, "Paratos: cannot load %s (GEMDOS error %ld).\n", filename, (long)result);
done:
	ptos_allocator_destroy();
	ptos_memory_destroy();
	return result < 0 ? 1 : result;
}
