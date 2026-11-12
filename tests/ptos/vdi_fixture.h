#pragma once
#include <stdint.h>
#include <string.h>

/* Classic 68k VDI wire arrays, independent of the emulator/native headers. */
static short control[12], integers[256], points[512], results[45], outpoints[12];
static struct { short *control, *intin, *ptsin, *intout, *ptsout; } block =
	{control, integers, points, results, outpoints};

static void request(short operation, short ni, short np, short handle)
{
	short subcode = control[5]; /* Classic bindings reuse this word. */
	memset(control, 0, sizeof(control));
	control[5] = subcode;
	control[0] = operation; control[1] = np; control[3] = ni; control[6] = handle;
}

static long call(void)
{
	register long d0 __asm__("d0") = -32;
	register void *d1 __asm__("d1") = &block;
	__asm__ volatile("move.w #0x73,%0\n\ttrap #2" : "+r"(d0), "+r"(d1) : : "d2", "a0", "a1", "a2", "cc", "memory");
	return d0;
}

static long open_workstation(void)
{
	request(100, 11, 0, 0);
	for (unsigned int i = 0; i < 10; i++) integers[i] = 1;
	integers[10] = 2;
	return call();
}
