#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mint/osbind.h>
#include <mint/mintbind.h>
#include <mint/basepage.h>

static char empty_tail[128];
enum { SS_OSFEATURES = 7 };

static int overlay(int mode)
{
	long available = (long)Malloc(-1L);
	BASEPAGE *bp = (BASEPAGE *)Pexec(3, "C:/OVERLAY.TOS", empty_tail, 0);
	if ((long)bp <= 0) return 1;
	void *environment = bp->p_env;
	long result = Pexec(mode, "", bp, 0);
	if (result != 23) return 2;
	if (mode == 4)
	{
		if (Mfree(environment) || Mfree(bp)) return 3;
	}
	else if (Mfree(bp) != -40L) return 4;
	return (long)Malloc(-1L) == available ? 0 : 5;
}

static long supervisor(void)
{
	volatile long preserved = 0x12345678L;
	unsigned short sr;
	__asm__ volatile("move.w %%sr,%0" : "=d"(sr));
	if (!(sr & 0x2000) || Super(1L) != -1L) return 1;
	char message[] = "guest supervisor I/O\n";
	if (Fwrite(1, sizeof(message) - 1, message) != sizeof(message) - 1) return 4;
	if (overlay(6)) return 2;
	if (preserved != 0x12345678L || Super(1L) != -1L) return 3;
	return 23;
}

static int native_launch(void)
{
	if (Dcreate("C:/PTOSDEMO") || Dsetpath("C:/PTOSDEMO")) return 1;
	long fd = Fcreate("shared.tmp", 0);
	if (fd < 0 || Fwrite((short)fd, 5, "guest") != 5) return 2;
	char tail[128] = {0};
	tail[0] = sprintf(tail + 1, "%ld", fd);
	long launched = Pexec(0, "C:/NATIVE.TOS", tail, 0);
	if (launched != 23) { printf("native Pexec result: %ld\n", launched); return 3; }
	/* Replacement is currently unavailable. It must not spawn another child. */
	if (Pexec(200, "C:/NATIVE.TOS", tail, 0) != -32L) return 4;
	if (Fseek(0, (short)fd, 0) != 0) return 5;
	char bytes[16] = {0};
	if (Fread((short)fd, sizeof(bytes), bytes) != 11 || strcmp(bytes, "guestnative")) return 6;
	if (Fclose((short)fd) || Fdelete("shared.tmp")) return 7;
	if (Dsetpath("C:/") || Ddelete("C:/PTOSDEMO")) return 8;
	return 0;
}

int main(void)
{
	const char *value = getenv("DEMO");
	if (!value || strcmp(value, "present")) return 1;
	if (Super(1L) != 0) return 2;
	if (Ssystem(SS_OSFEATURES, 0, 0) != 0) return 14;
	long old = Super(0L);
	long mode = Super(1L);
	Super(old);
	if (mode != -1 || Super(1L) != 0) return 3;
	if (*(volatile unsigned long *)0x42e != 0x01000000UL ||
		*(volatile unsigned long *)0x436 != 0x01000000UL) return 4;
	long cpu = -1;
	if (Ssystem(S_GETCOOKIE, 0x5f435055L, &cpu) || cpu != 20) return 5;
	volatile unsigned long *ticks = (volatile unsigned long *)0x4ba;
	unsigned long before = *ticks;
	unsigned long count;
	for (count = 0; count < 1000000UL && *ticks == before; count++) {}
	if (*ticks == before) return 6;
	if ((unsigned long)Ssystem(S_GETLVAL, 0x4baL, 0) < before) return 7;
	if ((unsigned char)Ssystem(S_GETBVAL, 0x4baL, 0) != *(volatile unsigned char *)0x4ba) return 8;
	if (Ssystem(S_SETLVAL, 0x4baL, 1) != -36L || Ssystem(S_SETBVAL, 0x4bbL, 1) != -25L) return 9;
	if (Ssystem(0x1234, 0, 0) != -32L) return 10;
	if (Supexec(supervisor) != 23 || Super(1L) != 0) return 11;
	for (int i = 0; i < 3; i++)
		if (overlay(4) || overlay(6)) return 12;
	int native = native_launch();
	if (native) { printf("native launch check: %d\n", native); return 13; }
	puts("pTOS guest semantics: PASS");
	return 0;
}
