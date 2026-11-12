#include <stdio.h>
#include <stdlib.h>
#include <mint/osbind.h>
#include <mint/basepage.h>
#include "vdi_fixture.h"

typedef struct form {
	void *address;
	short width, height, stride, standard, planes, reserved[3];
} form;
_Static_assert(sizeof(form) == 20, "classic MFDB size");
_Static_assert(sizeof(block) == 20 && sizeof(control) == 24, "classic VDI parameter/control size");

static void raster(form *source, form *destination)
{
	/* Pointers start at word 7, with no native alignment pad. */
	memcpy(&control[7], &source, sizeof(source));
	memcpy(&control[9], &destination, sizeof(destination));
}

int main(void)
{
	const char *mode = getenv("VDI");
	long result = open_workstation();
	if (mode && !strcmp(mode, "absent"))
	{
		if (result != -32L || control[6] != 0) return 1;
		puts("pTOS VDI unavailable: PASS");
		return 0;
	}
	if (result || control[6] <= 0 || control[4] != 45 || control[2] != 6 || results[0] <= 0 || results[1] <= 0) return 2;
	short handle = control[6];
	request(17, 1, 0, handle); integers[0] = 1;
	if (call() || control[4] != 1) return 3;
	request(6, 0, 2, handle);
	points[0] = 2; points[1] = 2; points[2] = 12; points[3] = 8;
	if (call() || control[4] || control[2]) return 4;
	request(8, 3, 1, handle);
	integers[0] = 'V'; integers[1] = 'D'; integers[2] = 'I'; points[0] = 2; points[1] = 20;
	if (call()) return 5;
	request(102, 1, 0, handle); integers[0] = 1;
	if (call() || control[4] != 45 || control[2] != 6) return 6;
	/* A one-pixel planar copy detects a missing big-endian WORD conversion. */
	unsigned short source_data[4] = {0x8000, 0x1357, 0x2468, 0xabcd};
	unsigned short destination_data[4] = {0, 0x1111, 0x2222, 0x3333};
	form source = {source_data, 16, 1, 1, 0, 1, {0}};
	form destination = {destination_data, 16, 1, 1, 0, 1, {0}};
	request(109, 1, 4, handle); integers[0] = 3;
	for (unsigned int i = 0; i < 8; i++) points[i] = 0;
	raster(&source, &destination);
	if (call() || destination_data[0] != 0x8000 || destination_data[1] != 0x1111 ||
		source.address != source_data || destination.address != destination_data) return 7;
	/* Two-plane standard -> device layout, including fd_stand writeback. */
	source_data[0] = 0x1234; source_data[1] = 0x5678; source_data[2] = 0x9abc; source_data[3] = 0xdef0;
	source.height = destination.height = 2; source.planes = destination.planes = 2;
	source.standard = destination.standard = 1;
	request(110, 0, 0, handle); raster(&source, &destination);
	if (call() || destination.standard != 0 || destination_data[0] != 0x1234 ||
		destination_data[1] != 0x9abc || destination_data[2] != 0x5678 || destination_data[3] != 0xdef0) return 8;
	request(17, 1, 0, handle); integers[0] = 1;
	short *saved = block.intout;
	block.intout = (short *)0x41000000UL;
	result = call(); block.intout = saved;
	if (result != -40L) return 9;
	request(125, 0, 0, handle);
	if (call() != -32L) return 10; /* no native registration of guest code */
	char tail[128] = {0};
	for (unsigned int cycle = 0; cycle < 32; cycle++)
		for (int mode = 4; mode <= 6; mode += 2)
		{
			BASEPAGE *child = (BASEPAGE *)Pexec(3, "C:/VDIOVL.TOS", tail, 0);
			if ((long)child <= 0) return 11;
			void *environment = child->p_env;
			long child_handle = Pexec(mode, "", child, 0);
			if (child_handle <= 0 || child_handle == handle) return 12;
			if (mode == 4 && (Mfree(environment) || Mfree(child))) return 13;
			request(17, 1, 0, (short)child_handle); integers[0] = 1;
			if (call() != -37L) return 14;
			request(17, 1, 0, handle);
			if (call()) return 15;
		}
	request(101, 0, 0, handle);
	if (call() || control[6]) return 16;
	puts("pTOS VDI marshalling: PASS");
	return 0;
}
