/* A real 68k shebang interpreter, selected through the kernel's PRG handler. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mint/basepage.h>

int main(int argc, char **argv)
{
	const char *value = getenv("DEMO");
	if (argc != 5 || strcmp(argv[1], "option with 'quotes'") ||
		strcmp(argv[2], "C:\\SCRIPT.SH") || strcmp(argv[3], "two words") || *argv[4]) return 1;
	if (!value || strcmp(value, "present") || (unsigned char)_base->p_cmdlin[0] != 127) return 2;
	FILE *script = fopen(argv[2], "rb");
	if (!script) return 3;
	static const char expected[] = "#!/BIN\\INTERP.TOS option with 'quotes'\r\n";
	char line[sizeof(expected) - 1];
	int valid = fread(line, 1, sizeof(line), script) == sizeof(line) &&
		!memcmp(line, expected, sizeof(line));
	if (fclose(script) || !valid) return 4;
	puts("68k shebang interpreter passed");
	return 42;
}
