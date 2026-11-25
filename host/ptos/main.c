#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "memory.h"
#include "guest.h"

extern char **environ;

static const ptos_guest_launch *guest_launch(void)
{
	char **end = environ;
	while (*end) end++;
	const uint32_t *aux = (const uint32_t *)(end + 1);
	for (unsigned int i = 0; i < 32 && aux[0]; i++, aux += 2)
		if (aux[0] == PTOS_AT_GUEST_LAUNCH)
			return (const ptos_guest_launch *)(uintptr_t)aux[1];
	return NULL;
}

static char *guest_environment(int argc, char **argv, const char *argv0)
{
	size_t size = 64; /* ARGV=NULL: indices, terminators */
	for (char **e = environ; *e; e++)
	{
		size_t n = strlen(*e) + 1;
		if (!**e || !strncmp(*e, "ARGV=", 5) || n > 32766 - size) return NULL;
		size += n;
	}
	for (int i = 0; i < argc; i++)
	{
		size_t n = strlen(i ? argv[i] : argv0) + 16;
		if (n > 32766 - size) return NULL;
		size += n;
	}
	char *env = calloc(1, size), *out = env;
	if (!env) return NULL;
	for (char **e = environ; *e; e++)
	{
		size_t n = strlen(*e) + 1;
		memcpy(out, *e, n); out += n;
	}
	memcpy(out, "ARGV=", 5); out += 5;
	int first_empty = 1;
	for (int i = 0; i < argc; i++)
	{
		if (*(i ? argv[i] : argv0)) continue;
		out += sprintf(out, first_empty ? "NULL:%d" : ",%d", i);
		first_empty = 0;
	}
	*out++ = 0;
	for (int i = 0; i < argc; i++)
	{
		const char *arg = i ? argv[i] : argv0;
		if (!*arg) arg = " ";
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
	const ptos_guest_launch *launch = guest_launch();
	const char *argv0 = launch && launch->argv0 ? (const char *)(uintptr_t)launch->argv0 : argv[1];
	const char *tail = launch ? (const char *)(uintptr_t)launch->tail : NULL;
	const char *env = launch ? (const char *)(uintptr_t)launch->environment : NULL;
	char *generated_env = NULL;
	char generated_tail[128];
	if (env && (launch->environment_size < 2 || launch->environment_size > PTOS_GUEST_ENV_MAX ||
		env[launch->environment_size - 2] || env[launch->environment_size - 1]))
	{
		result = PTOS_EINVAL;
		goto done;
	}
	if (!tail)
	{
		ptos_command_tail(generated_tail, argc - 2, argv + 2);
		tail = generated_tail;
	}
	if (!env)
	{
		generated_env = guest_environment(argc - 1, argv + 1, argv0);
		if (!generated_env) { result = PTOS_ERANGE; goto done; }
		env = generated_env;
	}
	char filename[1024];
	size_t path_length = strlen(argv[1]);
	if (path_length >= sizeof(filename)) { result = PTOS_ERANGE; goto done; }
	for (size_t i = 0; i <= path_length; i++) filename[i] = argv[1][i] == '/' ? '\\' : argv[1][i];
	result = ptos_load(filename, tail, env);
	free(generated_env);
	generated_env = NULL;
	if (result > 0) result = ptos_run_program(result);
	else fprintf(stderr, "Paratos: cannot load %s (GEMDOS error %ld).\n", filename, (long)result);
done:
	free(generated_env);
	ptos_allocator_destroy();
	ptos_memory_destroy();
	return result < 0 ? 1 : result;
}
