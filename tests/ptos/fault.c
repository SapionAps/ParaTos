#include <string.h>

int main(int argc, char **argv)
{
	if (argc != 2) return 1;
	if (!strcmp(argv[1], "read"))
		return *(volatile unsigned long *)0x41000000UL;
	/* A partial write to a live system variable must fault in the emulator,
	 * rather than falling through to a native low-memory store. */
	*(volatile unsigned char *)0x4bb = 1;
	return 2;
}
