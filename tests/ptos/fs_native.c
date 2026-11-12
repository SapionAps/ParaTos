/* Native comparison process: Pterm preserves the low 16 result bits. */
#include <mint/osbind.h>
#include <mint/mintbind.h>

#if defined(__arm__)
/* Independent native probe of the published ARM VDI SVC interface. */
static long native_vdi(void)
{
	struct { short words[8]; unsigned long pointers[3]; } control = {{100, 0, 0, 11, 0, 0, 0, 0}, {0}};
	short input[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2}, output[45], points[12];
	struct { void *control; short *intin, *ptsin, *intout, *ptsout; } block = {&control, input, 0, output, points};
	register long r0 __asm__("r0") = 0x73;
	register void *r1 __asm__("r1") = &block;
	__asm__ volatile("svc #2" : "+r"(r0), "+r"(r1) :
		: "r2", "r3", "r4", "r5", "r6", "ip", "lr", "cc", "memory");
	return r0;
}
#endif

int main(int argc, char **argv)
{
	if (argc != 2) return 1;
#if defined(__arm__)
	if (argv[1][0] == 'v') return (int)native_vdi();
#endif
	char buffer[128];
	if (argv[1][0] == 'x')
	{
		XATTR value;
		return (int)Fxattr(0, "C:\\FSCOMP\\A.TXT", &value);
	}
	if (argv[1][0] == 'p') return (int)Dpathconf("C:\\FSCOMP\\A.TXT", -1);
	if (argv[1][0] == 'n') return (int)Dgetcwd(buffer, 0, -1);
	if (argv[1][0] == 'r') return (int)Freadlink(-1, buffer, "C:\\FSCOMP\\A.TXT");
	if (argv[1][0] == 's') return (int)trap_1_w(0x150);
	return 2;
}
