#include <errno.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

#include "common.h"
#include "host.h"
#include "tasks.h"

static int32_t bconin(uint16_t device)
{
	if (device != 2) /* POSIX stdin represents the TOS console. */
		return TOS_ENOSYS;

	PosixTerminalLock();
	int fd = PosixHostFd(STDIN_FILENO);
	int32_t result;
	struct termios saved, raw;
	int restore = 0;
	if (tcgetattr(fd, &saved) == 0)
	{
		raw = saved;
		cfmakeraw(&raw);
		raw.c_cc[VMIN] = 1;
		raw.c_cc[VTIME] = 0;
		/* Do not discard characters already queued by the user. */
		if (tcsetattr(fd, TCSANOW, &raw) < 0)
		{
			result = MapErrno();
			goto out;
		}
		restore = 1;
	}
	else if (errno != ENOTTY)
	{
		result = MapErrno();
		goto out;
	}

	uint8_t input;
	ssize_t count;
	do {
		count = PosixRead(STDIN_FILENO, &input, 1);
	} while (count < 0 && errno == EINTR);
	result = count < 0 ? MapErrno() : (count ? input : 4);
	/* BIOS has no EOF result; expose redirected EOF as console Ctrl-D. */
	if (restore && tcsetattr(fd, TCSANOW, &saved) < 0)
		result = MapErrno();
out:
	PosixTerminalUnlock();
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
