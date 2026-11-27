#include <stdlib.h>
#include <string.h>
#include "guest_launch.h"
#include "tos_errors.h"

#define ENCODED_SIZE(n) (4 * (((n) + 2) / 3))
/* Keep encoded chunks below the POSIX per-argument limit. The native
 * pTOS environment fits in a single option. */
#define ENV_CHUNK_BYTES (48u * 1024)

static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void encode(char *out, const unsigned char *bytes, size_t size)
{
	for (size_t i = 0; i < size; i += 3)
	{
		unsigned a = bytes[i], b = i + 1 < size ? bytes[i + 1] : 0;
		unsigned c = i + 2 < size ? bytes[i + 2] : 0;
		*out++ = alphabet[a >> 2];
		*out++ = alphabet[(a & 3) << 4 | b >> 4];
		*out++ = i + 1 < size ? alphabet[(b & 15) << 2 | c >> 6] : '=';
		*out++ = i + 2 < size ? alphabet[c & 63] : '=';
	}
	*out = 0;
}

static int digit(char ch)
{
	const char *p = strchr(alphabet, ch);
	return p && *p ? (int)(p - alphabet) : -1;
}

static size_t decode(const char *encoded, unsigned char *out, size_t capacity)
{
	size_t length = strlen(encoded), used = 0;
	if (length % 4 || length > ENCODED_SIZE(capacity)) return 0;
	for (size_t i = 0; i < length; i += 4)
	{
		int a = digit(encoded[i]), b = digit(encoded[i + 1]);
		int c = digit(encoded[i + 2]), d = digit(encoded[i + 3]);
		if (a < 0 || b < 0 || used == capacity) return 0;
		out[used++] = (a << 2) | (b >> 4);
		if (c < 0)
			return encoded[i + 2] == '=' && encoded[i + 3] == '=' &&
				i + 4 == length && !(b & 15) ? used : 0;
		if (used == capacity) return 0;
		out[used++] = (b << 4) | (c >> 2);
		if (d < 0)
			return encoded[i + 3] == '=' && i + 4 == length && !(c & 3) ? used : 0;
		if (used == capacity) return 0;
		out[used++] = (c << 6) | d;
	}
	return used;
}

/* Keep the original ARGV prefix and any raw placeholders replaced by
 * NULL:index. An empty record reuses the corresponding command argument.
 * Duplicating every argument in the option would exceed Pexecve's byte
 * limit for a full-size guest environment. */
static size_t pack_environment(char *out, const guest_launch_args *args)
{
	size_t used = args->argv_offset;
	memcpy(out, args->environment, used);
	const char *raw = args->environment + used;
	for (int i = 0; i < args->argc; i++)
	{
		size_t size = strlen(raw) + 1;
		if (!strcmp(raw, args->argv[i]))
			out[used++] = 0;
		else
		{
			memcpy(out + used, raw, size);
			used += size;
		}
		raw += size;
	}
	return used;
}

int32_t guest_encode_options(const char *executable, const char *path,
	const guest_launch_args *args, char ***out)
{
	char *packed = NULL;
	const char *environment = args->environment;
	size_t env_size = args->environment_size;
	if (args->argv_offset)
	{
		packed = malloc(env_size);
		if (!packed) return TOS_ENSMEM;
		env_size = pack_environment(packed, args);
		environment = packed;
	}
	size_t chunks = (env_size + ENV_CHUNK_BYTES - 1) / ENV_CHUNK_BYTES;
	size_t slots = args->argc + 7 + 2 * chunks;
	char **argv = malloc(slots * sizeof(*argv) + ENCODED_SIZE(sizeof(args->tail)) + 1 +
		ENCODED_SIZE(env_size) + chunks);
	if (!argv)
	{
		free(packed);
		return TOS_ENSMEM;
	}
	char *tail_option = (char *)(argv + slots);
	encode(tail_option, (const unsigned char *)args->tail, sizeof(args->tail));
	argv[0] = (char *)executable;
	argv[1] = "--guest-prg";
	argv[2] = (char *)path;
	argv[3] = "--guest-tail";
	argv[4] = tail_option;
	int next = 5;
	char *env_option = tail_option + ENCODED_SIZE(sizeof(args->tail)) + 1;
	while (env_size)
	{
		size_t bytes = env_size > ENV_CHUNK_BYTES ? ENV_CHUNK_BYTES : env_size;
		encode(env_option, (const unsigned char *)environment, bytes);
		argv[next++] = args->argv_offset ? "--guest-argv-env" : "--guest-env";
		argv[next++] = env_option;
		env_option += ENCODED_SIZE(bytes) + 1;
		environment += bytes;
		env_size -= bytes;
	}
	argv[next++] = "--";
	if (args->argc) memcpy(argv + next, args->argv, args->argc * sizeof(*argv));
	argv[next + args->argc] = NULL;
	free(packed);
	*out = argv;
	return 0;
}

static size_t argv_prefix(const char *packet, size_t size)
{
	size_t pos = 0;
	while (pos < size && packet[pos])
	{
		const char *entry = packet + pos;
		const char *end = memchr(entry, 0, size - pos);
		if (!end) break;
		pos = end - packet + 1;
		if (!strcmp(entry, "ARGV") || !strncmp(entry, "ARGV=", 5)) return pos;
	}
	return 0;
}

static size_t unpack_environment(char *out, size_t limit, const char *packet, size_t size,
	int argc, char **argv)
{
	size_t prefix = argv_prefix(packet, size);
	if (!prefix) return 0;
	if (out) memcpy(out, packet, prefix);
	size_t used = prefix;
	for (int i = 0; i < argc; i++)
	{
		const char *record = packet + prefix;
		const char *end = memchr(record, 0, size - prefix);
		if (!end) return 0;
		prefix = end - packet + 1;
		const char *argument = *record ? record : argv[i];
		size_t bytes = strlen(argument) + 1;
		if (bytes == 1 || bytes >= limit - used) return 0;
		if (out) memcpy(out + used, argument, bytes);
		used += bytes;
	}
	if (out) out[used] = 0;
	return prefix == size ? used + 1 : 0;
}

/* Return the index of the original guest argv[0], or a GEMDOS error. */
int32_t guest_decode_options(int argc, char **argv, char tail[128], char **environment,
	size_t limit, size_t *length)
{
	if (argc < 7 || strcmp(argv[1], "--guest-prg") || !argv[2][0] ||
		strcmp(argv[3], "--guest-tail") ||
		decode(argv[4], (unsigned char *)tail, 128) != 128 ||
		(unsigned char)tail[0] > 127)
		return TOS_EINVAL;
	if (!strcmp(argv[5], "--")) return 6;
	int packed = !strcmp(argv[5], "--guest-argv-env");
	if ((!packed && strcmp(argv[5], "--guest-env")) || (packed && tail[0] != 127))
		return TOS_EINVAL;
	int end = 5;
	size_t size = 0;
	while (end + 1 < argc && !strcmp(argv[end], argv[5]))
	{
		const char *encoded = argv[end + 1];
		size_t chars = strlen(encoded);
		if (!chars || chars % 4 || chars > ENCODED_SIZE(ENV_CHUNK_BYTES)) return TOS_EINVAL;
		size_t bytes = chars / 4 * 3 - (encoded[chars - 1] == '=') - (encoded[chars - 2] == '=');
		if (bytes > limit - size) return TOS_EINVAL;
		size += bytes;
		end += 2;
	}
	if (end >= argc || strcmp(argv[end], "--") || (packed && end + 1 == argc))
		return TOS_EINVAL;
	char *raw = malloc(size);
	if (!raw) return TOS_ENSMEM;
	size_t used = 0;
	for (int i = 6; i < end; i += 2)
	{
		size_t bytes = decode(argv[i], (unsigned char *)raw + used, size - used);
		if (!bytes) { free(raw); return TOS_EINVAL; }
		used += bytes;
	}
	int first = end + 1;
	if (packed)
	{
		size_t bytes = unpack_environment(NULL, limit, raw, size, argc - first, argv + first);
		if (!bytes) { free(raw); return TOS_EINVAL; }
		char *expanded = malloc(bytes);
		if (!expanded) { free(raw); return TOS_ENSMEM; }
		unpack_environment(expanded, bytes, raw, size, argc - first, argv + first);
		free(raw);
		*environment = expanded;
		if (length) *length = bytes;
		return first;
	}
	if (size >= 2 && (raw[0] || !raw[1]))
		for (size_t i = 0; i + 1 < size; i++)
			if (!raw[i] && !raw[i + 1])
			{
				if (i + 2 != size) break;
				*environment = raw;
				if (length) *length = size;
				return first;
			}
	free(raw);
	return TOS_EINVAL;
}
