#include <string.h>
#include <mint/osbind.h>

int main(void)
{
	_DTA dta;
	Fsetdta(&dta);
	if (Fsfirst("C:/FSCOMP/A.TXT", 0) || strcmp(dta.dta_name, "A.TXT") || dta.dta_size != 7) return 1;
	if (Fsnext() != -49L) return 2;
	return 31;
}
