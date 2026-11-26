#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "host.h"

void ptos_command_tail(char tail[128], int argc, char **argv)
{
	int quoted = 0;
	for (int i = 0; i < argc; i++)
	{
		if (!*argv[i]) quoted = 1;
		for (const char *s = argv[i]; *s; s++)
			if (isspace((unsigned char)*s) || *s == '\'' || *s == '"') quoted = 1;
	}
	/* MagX convention: quote every argument if quoting is needed, and
	 * double embedded single quotes. ARGV remains the lossless source for
	 * quoted arguments and command lines longer than the classic tail. */
	memset(tail, 0, 128);
	size_t n = 0;
#define APPEND(ch) do { if (n < 126) tail[1 + n] = (ch); n++; } while (0)
	for (int i = 0; i < argc; i++)
	{
		if (i) APPEND(' ');
		if (quoted) APPEND('\'');
		for (const char *s = argv[i]; *s; s++)
		{
			APPEND(*s);
			if (quoted && *s == '\'') APPEND('\'');
		}
		if (quoted) APPEND('\'');
	}
#undef APPEND
	tail[0] = quoted || n > 126 ? 127 : (char)n;
}

char *ptos_guest_environment(int argc, char *const argv[], const char *argv0, char *const envp[])
{
	size_t env_size = 0, arg_size = 0, indices_size = 0;
	for (char *const *entry = envp; *entry; entry++)
	{
		size_t bytes = strlen(*entry) + 1;
		if (!**entry || !strncmp(*entry, "ARGV=", 5) || bytes > PTOS_GUEST_ENV_MAX - env_size)
			return NULL;
		env_size += bytes;
	}
	for (int i = 0; i < argc; i++)
	{
		size_t length = strlen(i ? argv[i] : argv0);
		size_t bytes = length ? length + 1 : 2;
		if (bytes > PTOS_GUEST_ENV_MAX - arg_size) return NULL;
		arg_size += bytes;
		if (!length)
		{
			size_t digits = 1;
			for (int index = i; index >= 10; index /= 10) digits++;
			indices_size += (indices_size ? 1 : 5) + digits;
		}
	}
	/* Include the marker's terminator and the final NUL. An empty
	 * classic environment needs both NUL bytes. */
	size_t suffix = env_size ? 1 : 2;
	if (argc) suffix = sizeof("ARGV=") + indices_size + arg_size + 1;
	if (suffix > PTOS_GUEST_ENV_MAX - env_size) return NULL;
	char *env = calloc(1, env_size + suffix), *out = env;
	if (!env) return NULL;
	for (char *const *entry = envp; *entry; entry++)
	{
		size_t bytes = strlen(*entry) + 1;
		memcpy(out, *entry, bytes);
		out += bytes;
	}
	if (!argc) return env;
	memcpy(out, "ARGV=", 5);
	out += 5;
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
		size_t bytes = strlen(arg) + 1;
		memcpy(out, arg, bytes);
		out += bytes;
	}
	*out = 0;
	return env;
}

#define PTOS_VECTOR_MAX 1024
#define PTOS_VECTOR_BYTES 65536u

/* Parse the classic tail when no explicit ARGV block was supplied. */
static int parse_tail(const char tail[128], char **argv, char *text)
{
	size_t length = (unsigned char)tail[0];
	size_t i = 1;
	int count = 0;

	/* A producer may leave padding after the historical CR terminator. */
	while (i <= length && tail[i] && tail[i] != '\r')
	{
		int quote = 0;
		if (isspace((unsigned char)tail[i])) { i++; continue; }
		argv[count++] = text;
		while (i <= length && tail[i] && tail[i] != '\r')
		{
			unsigned char ch = tail[i++];
			if (quote)
			{
				if (ch == quote)
				{
					if (i <= length && tail[i] == quote) i++;
					else { quote = 0; continue; }
				}
			}
			else if (ch == '\'' || ch == '"') { quote = ch; continue; }
			else if (isspace(ch)) break;
			*text++ = ch;
		}
		if (quote) return -1;
		*text++ = 0;
	}
	argv[count] = NULL;
	return count;
}

static int decode_empty_arguments(const char *metadata, char **argv, size_t argc)
{
	if (strncmp(metadata, "NULL:", 5)) return 1;
	metadata += 5;
	for (;;)
	{
		size_t index = 0;
		if (*metadata < '0' || *metadata > '9') return 0;
		do {
			index = 10 * index + (*metadata++ - '0');
			if (index >= argc) return 0;
		} while (*metadata >= '0' && *metadata <= '9');
		argv[index][0] = 0;
		if (!*metadata) return 1;
		if (*metadata++ != ',') return 0;
	}
}

int32_t ptos_capture_launch(const char *path, const char tail[128], const char *env,
	size_t env_size, int explicit_env, ptos_launch_args *out)
{
	size_t envc = 0, argc = 0, i;
	const char *cursor = env, *end = env + env_size;
	const char *metadata = NULL, *payload = NULL;
	const unsigned char tail_size = (unsigned char)tail[0];
	int32_t result = PTOS_EINVAL;
	char **envp, **argv;
	char *copy, *text;

	if (tail_size > 127) return PTOS_ERANGE;
	while (cursor < end && *cursor)
	{
		size_t size = strlen(cursor) + 1; /* guest_env bounded this block */
		if (!strcmp(cursor, "ARGV") || !strncmp(cursor, "ARGV=", 5))
		{
			if (explicit_env && tail_size == 127)
			{
				metadata = cursor[4] ? cursor + 5 : "";
				payload = cursor += size;
			}
			break; /* inherited ARGV belongs to the previous launch */
		}
		if (envc == PTOS_VECTOR_MAX) return PTOS_ERANGE;
		envc++;
		cursor += size;
	}
	if (payload)
	{
		while (cursor < end && *cursor)
		{
			if (argc == PTOS_VECTOR_MAX - envc) return PTOS_ERANGE;
			argc++;
			cursor += strlen(cursor) + 1;
		}
		if (!argc) return PTOS_EINVAL;
	}
	else
	{
		/* At most one argument per two bytes, plus the pathname. */
		argc = 1 + (tail_size + 1) / 2;
	}

	/* One allocation owns both vectors and the strings while the native
	 * scheduler can run another guest. Transport slots are always ILP32. */
	size_t slots = argc + envc + 2;
	size_t path_size = payload ? 0 : strlen(path) + 1;
	envp = malloc(slots * sizeof(*envp) + env_size + path_size + 128 +
		(explicit_env ? env_size : 0));
	if (!envp) return PTOS_ENSMEM;
	argv = envp + envc + 1;
	copy = (char *)(envp + slots);
	text = copy + env_size;
	memcpy(copy, env, env_size);
	for (i = 0; i < envc; i++)
	{
		envp[i] = copy;
		copy += strlen(copy) + 1;
	}
	envp[envc] = NULL;
	if (payload)
	{
		copy = (char *)(envp + slots) + (payload - env);
		for (i = 0; i < argc; i++)
		{
			argv[i] = copy;
			copy += strlen(copy) + 1;
		}
		argv[argc] = NULL;
		if (!decode_empty_arguments(metadata, argv, argc)) goto fail;
	}
	else
	{
		argv[0] = text;
		memcpy(text, path, path_size);
		int count = parse_tail(tail, argv + 1, text + path_size);
		if (count < 0) goto fail;
		argc = 1 + count;
	}
	if (envc + argc > PTOS_VECTOR_MAX ||
		(envc + argc + 2) * 4 + env_size + path_size + 128 > PTOS_VECTOR_BYTES)
	{
		result = PTOS_ERANGE;
		goto fail;
	}
	out->envp = envp;
	out->argv = argv;
	memcpy(out->tail, tail, sizeof(out->tail));
	out->environment = NULL;
	if (explicit_env)
	{
		char *raw = text + path_size + 128;
		memcpy(raw, env, env_size);
		out->environment = raw;
	}
	return 0;
fail:
	free(envp);
	return result;
}
