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
	int extended_argc = argc - 1;
	if (env && (launch->environment_size < 2 || launch->environment_size > PTOS_GUEST_ENV_MAX ||
		env[launch->environment_size - 2] || env[launch->environment_size - 1]))
	{
		result = PTOS_EINVAL;
		goto done;
	}
	if (!tail)
	{
		ptos_command_tail(generated_tail, argc - 2, argv + 2);
		/* Classic PRG startup supplies an empty argv[0]. */
		if (*argv0) generated_tail[0] = 127;
		if (generated_tail[0] != 127) extended_argc = 0;
		tail = generated_tail;
	}
	if (!env)
	{
		generated_env = ptos_guest_environment(extended_argc, argv + 1, argv0, environ);
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
