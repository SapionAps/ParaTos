#include "common.h"
#include "tos_errors.h"

/**
 * STEFcntrl - 24000
 *
 * SET_INFOTEXT (1)
 * special1 = (int8_t *) Pointer to new info-line
 * special2 = (int8_t *) Pointer to old info-line
 * 
 * The string pointed to be special1 will be adopted as the new info-line in
 * the status-line of the STEmulator. A value of NULL switches back to the
 * standard display again.
 * 
 * If a valid pointer to a buffer is passed in special2, this will contain
 * the old text of the info-line after the function call. With NULL, nothing
 * happens.
 * 
 * SET_WINDOWTITLE (2)
 * special1 = (int8_t *) Pointer to new title-line
 * special2 = (int8_t *) Pointer to old title-line
 * 
 * The string pointed to by special1 will be adopted as the new title-line of
 * the STEmulator window.
 * 
 * If a valid pointer to a buffer is passed in special2, this will contain
 * the old text of the window title after the function call. With NULL,
 * nothing happens.
 */
int32_t STEFcntrl ( int16_t func, int32_t special1, int32_t special2 )
{
	NOT_IMPLEMENTED(GDOS, STEFcntrl, 24000);
	return -TOS_ENOSYS;
}

/**
 * Salert - 316
 *
 * The function Salert outputs an alert or error-message that is written to
 * the alert pipeline U:\PIPE\ALERT.
 * 
 * The message msg should not contain any escape or control characters,
 * linefeeds etc. It should be a simple one-line NULL-terminated character
 * string alert- or error-message.
 * 
 * The function formats the message itself and sends it to the user. The
 * exact format of the output however depends on the system configuration in
 * use.
 *
 * void Salert ( int8_t *msg )
 */
void Salert ( emuptr32_t msg )
{
	NOT_IMPLEMENTED(GDOS, Salert, 316);
}

/**
 * Sconfig - 51
 *
 * The function Sconfig permits the configuration of parts of the operating
 * system. The following apply: Parameter Meaning
 */
int32_t Sconfig ( int16_t mode, int32_t flags )
{
	NOT_IMPLEMENTED(GDOS, Sconfig, 51);
	return -TOS_ENOSYS;
}

/**
 * Shutdown - 337
 *
 * The function Shutdown kills all processes, syncs file-systems then halts
 * or reboots the system.
 * 
 * On mode equal to SHUT_HALT (0L), the system will shut down, then enter a
 * halted condition.
 * 
 * On mode equal to SHUT_BOOT (1L), the system will reboot the machine after
 * shutting everything down.
 * 
 * On mode equal to SHUT_COLD (2L), the system will act the same as with the
 * SHUT_BOOT mode, except that a cold start rather than the warm start will
 * be performed. SHUT_COLD mode is recognized as of FreeMiNT version 1.15.5;
 * older versions of the kernel will treat the SHUT_COLD mode as SHUT_BOOT.
 * 
 * On mode equal to SHUT_POWER (3L), the system will turn the power off. The
 * only hardware that supports it at present is CT60. If the hardware does
 * not support it, SHUT_HALT will be performed. SHUT_POWER mode is recognized
 * as of FreeMiNT version 1.16a; older versions of the kernel will treat the
 * SHUT_POWER mode as SHUT_COLD.
 * 
 * All other values of mode are reserved for future definition.
 * 
 * Older versions of MiNT contained a bug that might cause the system to
 * crash if you called Shutdown while both GEM AES and virtual console daemon
 * were present.
 * 
 * You need root privileges to shut the system down.
 */
void Shutdown ( int32_t mode )
{
	NOT_IMPLEMENTED(GDOS, Shutdown, 337);
}

/**
 * Slbclose - 23
 *
 * The function Slbclose closes a shared library.
 * 
 * The parameter sl is the descriptor that is obtained with Slbclose.
 *
 * int32_t Slbclose( SHARED_LIB *sl )
 */
int32_t Slbclose( emuptr32_t sl )
{
	NOT_IMPLEMENTED(GDOS, Slbclose, 23);
	return -TOS_ENOSYS;
}

/**
 * Slbopen - 22
 *
 * The function Slbopen opens a shared library. Parameter Meaning
 *
 * int32_t Slbopen( int8_t *name, int8_t *path, int32_t min_ver, SHARED_LIB *sl, SLB_EXEC *fn )
 */
int32_t Slbopen( emuptr32_t name, emuptr32_t path, int32_t min_ver, emuptr32_t sl, emuptr32_t fn )
{
	NOT_IMPLEMENTED(GDOS, Slbopen, 22);
	return -TOS_ENOSYS;
}

/**
 * Srealloc - 21
 *
 * The GEMDOS routine Srealloc allocates a block of length len bytes for the
 * screen memory.
 * 
 * The screen memory is a block of the ST-RAM whose owner is the boot
 * process. The address of the screen memory (logbase or physbase) is not
 * affected by this function.
 */
int32_t Srealloc ( int32_t len )
{
	NOT_IMPLEMENTED(GDOS, Srealloc, 21);
	return -TOS_ENOSYS;
}

/**
 * Ssync - 336
 *
 * The function Ssync synchronises all mounted file-systems.
 * 
 * Warning: In MiNT this function is designated as Sync.
 */
void Ssync ( void )
{
	NOT_IMPLEMENTED(GDOS, Ssync, 336);
}

/**
 * Ssystem - 340
 *
 * The Ssystem call has been designed to make your life easier. Using this
 * you can get some closer control of the system and the kernel itself. Via
 * this call the kernel now supports e.g. an easy cookie jar management and
 * provides a safe access to supervisor memory. It's strictly encouraged to
 * access GEMDOS variables and system vectors via the Ssystem, because this
 * way is considered safe for multi-user setups.
 * 
 * arg1 and arg2 are LONG parameters specific to a particular mode. If a mode
 * doesn't use a parameter, it is usually ignored, but should be set to zero
 * for future compatibility. mode specifies a particular action as follows:
 * mode Meaning 
 */
int32_t Ssystem ( int16_t mode, int32_t arg1, int32_t arg2 )
{
	NOT_IMPLEMENTED(GDOS, Ssystem, 340);
	return -TOS_ENOSYS;
}

/**
 * Super - 32
 *
 * The GEMDOS routine Super switches between user- and supervisor-mode, or
 * interrogates the current operating
 * 
 * The address of the supervisor stack may be altered during switching. If
 * one passes the value 1 for the parameter stack, then the current mode will
 * be interrogated. The value 0 switches between user- and supervisor-mode.
 * All other values will be regarded as the new address for the supervisor
 * stack. In the supervisor-mode one can access all protected memory blocks.
 * 
 * Note: With regard to future operating system versions, you should avoid
 * use of the Super function. This function gives rise to major problems in
 * conjunction with virtual memory and interruptible (and re-entrant)
 * file-systems in a multitasking system. If neccesary, alter your code so
 * that you can use the BIOS function Supexec.
 * 
 * For MiNT the following applies:
 * If you are in SECURELEVEL > 1, you must have root privileges to get into
 * supervisor-mode. If you don't have them and call Super, the SIGSYS signal
 * (12) will be raised.
 *
 * int32_t Super ( void *stack )
 */
int32_t Super ( emuptr32_t stack )
{
	NOT_IMPLEMENTED(GDOS, Super, 32);
	return -TOS_ENOSYS;
}

/**
 * Suptime - 319
 *
 * The function Suptime returns the current uptime and load averages from the
 * system.
 * 
 * uptime is a pointer to a LONG value that will be filled with the current
 * amount of seconds elapsed since the last reboot.
 * 
 * loadaverage is a pointer to a three LONGword array, that will be filled
 * with the current load averages (for the last 1, 5 and 15 minutes).
 * 
 * The load average value is calculated using the following formula:
 * 
 * sum += (new_load - old_load) * LOAD_SCALE;
 * load_avg = sum / MAX_SIZE;
 * 
 * where LOAD_SCALE is 2048, MAX_SIZE is the number of 5 second periods in
 * the specified amount of time (12 for 1 min., 60 for 5 min. and 180 for 15
 * min). new_load is the number of currently running processes, old_load is
 * the number of processes running previous time. A 'running' process is
 * considered the one which is put onto run or ready queue.
 *
 * int32_t Suptime ( int32_t *uptime, int32_t *loadaverage )
 */
int32_t Suptime ( emuptr32_t uptime, emuptr32_t loadaverage )
{
	NOT_IMPLEMENTED(GDOS, Suptime, 319);
	return -TOS_ENOSYS;
}

/**
 * Sversion - 48
 *
 * The GEMDOS routine Sversion obtains the version number of the GEMDOS. At
 * the present time, the following versions are known: Version Explanation
 */
uint16_t Sversion ( void )
{
	NOT_IMPLEMENTED(GDOS, Sversion, 48);
	return -TOS_ENOSYS;
}

/**
 * Syield - 255
 *
 * By calling the Syield function a process releases the CPU, so that it can
 * be allocated to other processes (possibly earlier than planned).
 * 
 * Note: Under MagiC this function directly calls the AES routine appl_yield,
 * and will be ignored in the Auto folder.
 */
void Syield ( void )
{
	NOT_IMPLEMENTED(GDOS, Syield, 255);
}

/**
 * Sysconf - 290
 *
 * The function Sysconf returns information about the capabilities (or the
 * configuration) of the operating system. With the parameter n various
 * values can be interrogated: n Meaning
 */
int32_t Sysconf ( int16_t n )
{
	NOT_IMPLEMENTED(GDOS, Sysconf, 290);
	return -TOS_ENOSYS;
}
