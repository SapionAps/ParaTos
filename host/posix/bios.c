#include <errno.h>
#include <unistd.h>

#include "common.h"
#include "host.h"
#include "tasks.h"

static int32_t bconin(uint16_t device)
{
	if (device != 2) /* POSIX stdin represents the TOS console. */
		return TOS_ENOSYS;

	int session = PosixTerminalBegin(PosixHostFd(STDIN_FILENO));
	if (session < 0) return MapErrno();

	uint8_t input;
	ssize_t count;
	do {
		count = PosixRead(STDIN_FILENO, &input, 1);
	} while (count < 0 && errno == EINTR);
	int32_t result = count < 0 ? MapErrno() : (count ? input : 4);
	/* BIOS has no EOF result; expose redirected EOF as console Ctrl-D. */
	if (session && PosixTerminalEnd() < 0) result = MapErrno();
	return result;
}

void dispatch_bios_trap(void)
{
	uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
	uint16_t num = m68k_read_memory_16(sp);
	int32_t result = -(int32_t)num;

	if (num == 2)
		result = bconin(m68k_read_memory_16(sp+2));
	else if (num == 3)
		PosixLog(1, "BCONOUT(%d, %c)\n", m68k_read_memory_16(sp-2), 0x7f&m68k_read_memory_16(sp-4));
	else
		PosixLog(1, "BIOS(0x%02x)\n", num);

	m68k_set_reg(M68K_REG_D0, (uint32_t)result);
}
