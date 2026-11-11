#include "common.h"
#include "host.h"

/**
 * Nversion - 96
 *
 * Returns the identifier of the connected network
 */
int32_t Nversion( void )
{
	NOT_IMPLEMENTED(GEMDOS, Nversion, 96);
	return TOS_ENOSYS;
}
