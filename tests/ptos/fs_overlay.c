#include <string.h>
#include <mint/osbind.h>

int main(void)
{
	_DTA dta;
	Fsetdta(&dta);
	if (Fsfirst("C:/FSCOMP/A.TXT", 0) || strcmp(dta.dta_name, "A.TXT") || dta.dta_size != 7) return 1;
	if (Fsnext() != -49L) return 2;
	/* The parent holds two native cursors. Fill the remaining slots with
	 * unfinished searches, then verify that pressure cannot evict them. */
	_DTA unfinished[7];
	for (unsigned int i = 0; i < 7; i++)
	{
		memset(&unfinished[i], 0x5a, sizeof(unfinished[i]));
		Fsetdta(&unfinished[i]);
		long result = Fsfirst("C:/FSCOMP/*.TXT", 0);
		if (i < 6) { if (result) return 3; }
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
