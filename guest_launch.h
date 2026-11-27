#pragma once
#include <stddef.h>
#include <stdint.h>

typedef struct {
	char **argv;
	char **envp; /* owns both pointer arrays and all strings */
	char tail[128];
	const char *environment; /* optional exact guest block in the same allocation */
	size_t environment_size, argv_offset;
	int argc, envc;
} guest_launch_args;

void guest_command_tail(char tail[128], int argc, char **argv);
/* The caller frees the flat block. Zero argc encodes only the environment. */
char *guest_build_environment(int argc, char *const argv[], const char *argv0,
	char *const envp[], size_t limit, size_t *length);
int32_t guest_capture_launch(const char *path, const char tail[128], const char *env,
	size_t env_size, int explicit_env, guest_launch_args *out);
int32_t guest_encode_options(const char *executable, const char *path,
	const guest_launch_args *args, char ***out);
int32_t guest_decode_options(int argc, char **argv, char tail[128], char **environment,
	size_t limit, size_t *length);
