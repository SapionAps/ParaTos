#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"
#include "memory.h"
#include "guest.h"

extern char **environ;

static uintptr_t auxiliary(uint32_t tag)
{
	char **end = environ;
	while (*end) end++;
	const uint32_t *aux = (const uint32_t *)(end + 1);
	for (unsigned int i = 0; i < 32 && aux[0]; i++, aux += 2)
		if (aux[0] == tag) return aux[1];
	return 0;
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
	enum { AT_EXECFN = 31, AT_PTOS_LAUNCH_VECTORS = 0x5004 };
	const uint32_t *launch = (const uint32_t *)auxiliary(AT_PTOS_LAUNCH_VECTORS);
	const char *const *original = launch ? (const char *const *)(uintptr_t)launch[1] : NULL;
	int original_argc = 0;
	if (original)
		while (original[original_argc]) original_argc++;
	/* Binfmt supplies the interpreter's path in argv[0]. Direct Pexecve
	 * permits a caller-chosen argv[0], so use the executable pathname. */
	int redirected = original && argc > original_argc;
	const char *executable = redirected ? argv[0] : (const char *)auxiliary(AT_EXECFN);
	int32_t result = ptos_set_executable(executable ? executable : argv[0]);
	if (result < 0)
	{
		fprintf(stderr, "Paratos: cannot resolve its executable path (GEMDOS error %ld).\n", (long)result);
		return 1;
	}
	result = ptos_memory_init();
	if (result < 0)
	{
		fprintf(stderr, "Paratos: cannot initialize guest RAM (GEMDOS error %ld); Ssystem reads and low fixed-mapping kernel support are required.\n", (long)result);
		return 1;
	}
	ptos_allocator_init();
	char *env = NULL;
	char tail[128];
	const char *program = argv[1];
	int guest_index = 1;
	const char *argv0;
	int guest_options = !strcmp(argv[1], "--guest-prg");
	if (guest_options)
	{
		result = guest_decode_options(argc, argv, tail, &env, PTOS_GUEST_ENV_MAX, NULL);
		if (result < 0)
		{
			fprintf(stderr, "Paratos: invalid guest launch options (GEMDOS error %ld).\n", (long)result);
			goto done;
		}
		guest_index = result;
		program = argv[2];
		argv0 = env ? NULL : argv[guest_index];
	}
	else
	{
		argv0 = redirected && original_argc && argc == original_argc + 1 ? original[0] : program;
		guest_command_tail(tail, argc - 2, argv + 2);
		/* Classic PRG startup supplies an empty argv[0]. */
		if (*argv0) tail[0] = 127;
	}
	if (!env)
	{
		int extended_argc = guest_options || tail[0] == 127 ? argc - guest_index : 0;
		env = guest_build_environment(extended_argc, argv + guest_index, argv0,
			environ, PTOS_GUEST_ENV_MAX, NULL);
		if (!env) { result = PTOS_ERANGE; goto done; }
	}
	char filename[PTOS_PATH_MAX];
	size_t path_length = strlen(program);
	if (path_length >= sizeof(filename)) { result = PTOS_ERANGE; goto done; }
	for (size_t i = 0; i <= path_length; i++) filename[i] = program[i] == '/' ? '\\' : program[i];
	result = ptos_load(filename, tail, env);
	free(env);
	env = NULL;
	if (result > 0) result = ptos_run_program(result);
	else fprintf(stderr, "Paratos: cannot load %s (GEMDOS error %ld).\n", filename, (long)result);
done:
	free(env);
	ptos_allocator_destroy();
	ptos_memory_destroy();
	return result < 0 ? 1 : result;
}
