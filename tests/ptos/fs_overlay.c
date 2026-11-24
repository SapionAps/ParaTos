#include <stdio.h>
#include <string.h>
#include <mint/osbind.h>

int main(void)
{
	_DTA dta;
	Fsetdta(&dta);
	if (Fsfirst("C:/FSCOMP/A.TXT", 0) || strcmp(dta.dta_name, "A.TXT") || dta.dta_size != 7) return 1;
	if (Fsnext() != -49L) return 2;
	/* A cloned PROCESS can start more searches than an in-place overlay
	 * sharing the parent's two active cursors. Fill the available budget
	 * and leave it occupied so the parent can check its cursors on exit. */
#ifdef PTOS_TEST_MANAGED_KERNEL
	enum { minimum = 7 };
#else
	enum { minimum = 6 };
#endif
	_DTA unfinished[9];
	for (unsigned int i = 0; i < sizeof(unfinished) / sizeof(*unfinished); i++)
	{
		memset(&unfinished[i], 0x5a, sizeof(unfinished[i]));
		Fsetdta(&unfinished[i]);
		long result = Fsfirst("C:/FSCOMP/*.TXT", 0);
		if (!result) continue;
		if (i < minimum || result != -39L)
		{
			printf("filesystem cursor %u: %ld\n", i + 1, result);
			return 3;
		}
		const unsigned char *bytes = (const unsigned char *)&unfinished[i];
		for (unsigned int j = 0; j < sizeof(unfinished[i]); j++)
			if (bytes[j] != 0x5a) return 5;
		return 31;
	}
	return 4;
}
