/* Ordinary 68k CLI filesystem calls through the native pTOS adapter. */
#include <stdio.h>
#include <string.h>
#include <mint/osbind.h>
#include <mint/mintbind.h>

_Static_assert(sizeof(_DTA) == 44, "68k DTA ABI");
_Static_assert(sizeof(XATTR) == 52, "68k XATTR ABI");

static int create(const char *path, const char *data, long size)
{
	long file = Fcreate(path, 0);
	if (file < 0) return 1;
	_DOSTIME date = {0x1234, 0x5821};
	long written = Fwrite((short)file, size, data);
	long dated = Fdatime(&date, (short)file, 1);
	long closed = Fclose((short)file);
	return written != size || dated || closed;
}

static int valid(const _DTA *dta)
{
	if (dta->dta_time != 0x1234 || dta->dta_date != 0x5821) return 0;
	if (!strcmp(dta->dta_name, "A.TXT")) return dta->dta_size == 7;
	if (!strcmp(dta->dta_name, "B.TXT")) return dta->dta_size == 11;
	return 0;
}

static int same_native(char operation, long guest_result)
{
	char tail[128] = {1, 0};
	tail[1] = operation;
	long native_result = Pexec(0, "C:/FSNATIVE.TOS", tail, 0);
	/* Native child exit status is the 16-bit Pterm contract. Injected tests
	 * additionally assert that full 32-bit errors reach the guest's D0. */
	return (unsigned short)guest_result == (unsigned short)native_result;
}

int main(void)
{
	if (Dcreate("C:/FSCOMP")) return 1;
	if (create("C:/FSCOMP/A.TXT", "1234567", 7) ||
		create("C:/FSCOMP/B.TXT", "12345678901", 11)) return 2;
	_DISKINFO disk;
	if (Dfree(&disk, 3) || disk.b_free <= 0 || disk.b_free > disk.b_total ||
		disk.b_secsiz != 512 || disk.b_clsiz <= 0) return 3;
	_DTA first, second, saved;
	_DTA *original = Fgetdta();
	Fsetdta(&first);
	if (Fsfirst("C:/FSCOMP/*.TXT", 0) || !valid(&first)) return 4;
	saved = first;
	Fsetdta(&second);
	if (Fsfirst("C:/FSCOMP/A.TXT", 0) || strcmp(second.dta_name, "A.TXT") ||
		second.dta_size != 7) return 5;
	/* A nested overlay owns its DTA and search without replacing the
	 * parent's native search cursor. The overlay deliberately leaves it set. */
	char tail[128] = {0};
	void *child = (void *)Pexec(3, "C:/FSOVL.TOS", tail, 0);
	if ((long)child <= 0 || Pexec(6, "", child, 0) != 31) return 6;
	if (Fgetdta() != &second) return 7;
	Fsetdta(&first);
	XATTR metadata;
	if (!same_native('x', Fxattr(0, "C:/FSCOMP/A.TXT", &metadata)) ||
		!same_native('p', Dpathconf("C:/FSCOMP/A.TXT", -1)) ||
		!same_native('s', trap_1_w(0x150))) return 8;
	if (Fsnext() || !valid(&first) || !strcmp(first.dta_name, saved.dta_name)) return 9;
	saved = first;
	if (Fsnext() != -49L || memcmp(&first, &saved, sizeof(first))) return 10;
	Fsetdta(&second);
	if (Fsnext() != -49L) return 11;
	char directory[128], classic[128];
	long cwd = Dgetcwd(directory, 0, sizeof(directory));
	if (cwd || Dgetpath(classic, 0) || strcmp(directory, classic)) return 12;
	Fsetdta(original);
	if (Fdelete("C:/FSCOMP/A.TXT") || Fdelete("C:/FSCOMP/B.TXT") ||
		Ddelete("C:/FSCOMP")) return 13;
	puts("pTOS filesystem compatibility: PASS");
	return 0;
}
