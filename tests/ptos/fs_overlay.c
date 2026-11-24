#include <string.h>
#include <mint/osbind.h>

int main(void)
{
	_DTA dta;
	Fsetdta(&dta);
	if (Fsfirst("C:/FSCOMP/A.TXT", 0) || strcmp(dta.dta_name, "A.TXT") || dta.dta_size != 7) return 1;
	if (Fsnext() != -49L) return 2;
	/* A cloned PROCESS has its own eight slots. The legacy in-place
	 * overlay shares the table with the parent's two active searches. */
#ifdef PTOS_TEST_MANAGED_KERNEL
	enum { available = 8 };
#else
	enum { available = 6 };
#endif
	_DTA unfinished[available + 1];
	for (unsigned int i = 0; i <= available; i++)
	{
		memset(&unfinished[i], 0x5a, sizeof(unfinished[i]));
		Fsetdta(&unfinished[i]);
		long result = Fsfirst("C:/FSCOMP/*.TXT", 0);
		if (i < available) { if (result) return 3; }
		else
		{
			if (result != -39L) return 4;
			const unsigned char *bytes = (const unsigned char *)&unfinished[i];
			for (unsigned int j = 0; j < sizeof(unfinished[i]); j++)
				if (bytes[j] != 0x5a) return 5;
		}
	}
	return 31;
}
