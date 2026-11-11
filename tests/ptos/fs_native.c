/* Native comparison process: Pterm preserves the low 16 result bits. */
#include <mint/osbind.h>
#include <mint/mintbind.h>

int main(int argc, char **argv)
{
	if (argc != 2) return 1;
	if (argv[1][0] == 'x')
	{
		XATTR value;
		return (int)Fxattr(0, "C:\\FSCOMP\\A.TXT", &value);
	}
	if (argv[1][0] == 'p') return (int)Dpathconf("C:\\FSCOMP\\A.TXT", -1);
	if (argv[1][0] == 's') return (int)trap_1_w(0x150);
	return 2;
}
