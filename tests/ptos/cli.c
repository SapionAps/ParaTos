/* A real 68k CLI program; link with libcmini for the integration check in
 * host/ptos/README.txt. No Paratos-specific guest API is used. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mint/basepage.h>
#include <mint/osbind.h>
#include <mint/mintbind.h>

int main(int argc, char **argv)
{
	const char *env = getenv("DEMO");
#ifdef PTOS_TEST_EXEC_REPLACEMENT
	if (env && !strcmp(env, "replace-prg"))
	{
		char tail[128] = "\005child", environment[64] = {0};
		snprintf(environment, sizeof(environment) - 1, "CHECK_PRG_PID=%d", (int)Pgetpid());
		printf("PRG replacement returned: %ld\n", Pexec(200, "C:\\CLI.TOS", tail, environment));
		return 13;
	}
	const char *pid = getenv("CHECK_PRG_PID");
	if (pid) return atoi(pid) == Pgetpid() && argc == 2 && !strcmp(argv[1], "child") ? 44 : 14;
#endif
	if (env && !strcmp(env, "relaunch"))
	{
		static const char handler[] = "C:\\BIN\\PARATOS.PRG", saved[] = "C:\\BIN\\PARATOS.BAK";
		char tail[128] = "\014plain  again";
		tail[127] = 0x5a;
		if (Frename(0, handler, saved)) return 11;
		long result = Pexec(0, "C:\\CLI.TOS", tail, "DEMO=rawshort\0");
		if (Frename(0, saved, handler) || result != 39) return 12;
		return 43;
	}
	if (env && !strcmp(env, "inherit-limit"))
	{
		const char *padding = getenv("PAD");
		size_t size = padding ? strlen(padding) : 0;
		if (size < 32700 || padding[0] != 'x' || padding[size - 1] != 'z') return 8;
		if (argc == 1)
		{
			static const char tail[128] = "\005child";
			long result = Pexec(0, "C:\\CLI.TOS", tail, NULL);
			if (result == 41) return 42;
			printf("CLI inherited environment child result: %ld\n", result);
			return 10;
		}
		return argc == 2 && !strcmp(argv[1], "child") && _base->p_cmdlin[0] == 5 ? 41 : 8;
	}
	if (env && !strcmp(env, "rawshort"))
	{
		char expected[128] = "\014plain  again";
		expected[127] = 0x5a;
		return argc == 3 && !strcmp(argv[1], "plain") && !strcmp(argv[2], "again") &&
			!memcmp(_base->p_cmdlin, expected, sizeof(expected)) ? 39 : 8;
	}
	if (env && !strcmp(env, "rawextended"))
	{
		char expected[128] = "\177'raw  display'\rpadding";
		expected[127] = 0x5a;
		return argc == 3 && !strcmp(argv[0], "original guest name") &&
			!strcmp(argv[1], "two words") && !*argv[2] &&
			!memcmp(_base->p_cmdlin, expected, sizeof(expected)) ? 40 : 8;
	}
	if (!env || strcmp(env, "present")) return 2;
	const char *expected_name = getenv("CHECKARGV0");
	if (expected_name && strcmp(argv[0], expected_name)) return 9;
	int marker = expected_name && *expected_name ? 127 : 0;
	if (argc == 1) return _base->p_cmdlin[0] != marker || _base->p_cmdlin[1] ? 8 : 38;
	if (argc == 2 && !strcmp(argv[1], "plain"))
		return _base->p_cmdlin[0] != (marker ? marker : 5) ||
			strcmp(_base->p_cmdlin + 1, "plain") ? 8 : 38;
	if (argc != 3 || strcmp(argv[1], "two words") || strcmp(argv[2], "")) return 1;
	if (((unsigned char)_base->p_cmdlin[0] != 127 && _base->p_cmdlin[0] != 14) ||
		strncmp(_base->p_cmdlin + 1, "'two words' ''", 14)) return 8;
	FILE *file = fopen("cli-data.tmp", "wb");
	if (!file) return 3;
	if (fwrite("guest data", 1, 10, file) != 10 || fclose(file)) return 4;
	file = fopen("cli-data.tmp", "rb");
	if (!file) return 5;
	char bytes[11] = {0};
	if (fread(bytes, 1, 10, file) != 10 || fclose(file) || strcmp(bytes, "guest data")) return 6;
	if (remove("cli-data.tmp")) return 7;
	puts("CLI fixture passed");
	return 37;
}
