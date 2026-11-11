#include "common.h"
#include "gemdos.h"

emuptr32_t current_process;

/**
 * Pdomain - 281
 *
 * The function Pdomain serves for setting or obtaining the domain in which
 * the process is executed. This domain can influence the behaviour of the
 * process drastically in parts. The following apply: dom Meaning
 */
int16_t Pdomain ( int16_t dom )
{
	if (dom > -1)
	{
		if (dom > 1)
		{
			return -1;
		}
		else
		{
			m68k_write_field(current_process, basepage_t, mint_domain, dom);
		}
	}

	return m68k_read_field(current_process, basepage_t, mint_domain);
}

/**
 * Fgetdta - 47
 *
 * The GEMDOS routine Fgetdta obtains the address of a buffer for Fsfirst and
 * Fsnext.
 *
 * DTA *Fgetdta ( void )
 */
emuptr32_t Fgetdta ( void )
{
	return m68k_read_field(current_process, basepage_t, p_dta);
}

/**
 * Fsetdta - 26
 *
 * The GEMDOS routine Fsetdta sets the address of the memory segment that
 * will be used as the new DTA. The parameter buf is a pointer to this
 * structure.
 *
 * Note: At program launch the DTA will be established starting at offset
 * 0x80 in the basepage.
 *
 * void Fsetdta ( DTA *buf )
 */

void Fsetdta ( emuptr32_t buf )
{
	m68k_write_field(current_process, basepage_t, p_dta, buf);
}

