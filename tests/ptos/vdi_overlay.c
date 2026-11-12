#include "vdi_fixture.h"

int main(void)
{
	long result = open_workstation();
	if (result || control[6] <= 0) return 1;
	/* Leave the workstation open: termination must close it even for PE_GO. */
	return control[6];
}
