#include <termios.h>
#include <unistd.h>
#include "common.h"
#include "tos_errors.h"

/**
 * Cauxin - 3
 *
 * The GEMDOS routine Cauxin reads a character byte from the GEMDOS handle 2
 * - normally the serial port aux:. The function waits until the character
 * arrives.
 *
 * Note: Atari recommends use of the BIOS function Bconin for this, as Cauxin
 * can cause problems when its handle is redirected and end-of-file is
 * encountered.
 */
int32_t Cauxin ( void )
{
	NOT_IMPLEMENTED(GEMDOS, Cauxin, 3);
	return TOS_ENOSYS;
}

/**
 * Cauxis - 18
 *
 * The GEMDOS routine Cauxis checks the status of GEMDOS handle 2 - normally
 * the serial port aux: - to see whether at least one character is waiting to
 * be received.
 *
 * Note: The function works correctly with redirection of input/output only
 * as of GEMDOS Version 0.15. Atari recommends the use of the BIOS function
 * Bconstat instead.
 */
int16_t Cauxis ( void )
{
	NOT_IMPLEMENTED(GEMDOS, Cauxis, 18);
	return TOS_ENOSYS;
}

/**
 * Cauxos - 19
 *
 * The GEMDOS routine Cauxos checks the status of GEMDOS handle 2 - normally
 * the serial port aux: - to see whether it is ready to output characters.
 *
 * Note: Atari recommends use of the BIOS function Bcostat for this.
 */
int16_t Cauxos ( void )
{
	NOT_IMPLEMENTED(GEMDOS, Cauxos, 19);
	return TOS_ENOSYS;
}

/**
 * Cauxout - 4
 *
 * The GEMDOS routine Cauxout writes the character c to GEMDOS handle 2 -
 * normally the serial port aux:.
 *
 * Note: The function works correctly with redirection of input/output only
 * as of GEMDOS Version 0.15. Atari recommends the use of the BIOS function
 * Bconout instead.
 */
int32_t Cauxout ( int16_t c )
{
	NOT_IMPLEMENTED(GEMDOS, Cauxout, 4);
	return TOS_ENOSYS;
}

/**
 * Cconin - 1
 *
 * The GEMDOS routine Cconin reads a character from GEMDOS handle 0 -
 * normally the standard input device con: (the keyboard as a rule), waiting
 * until one is available.
 *
 * Note: By setting bit 3 of the system variable conterm one can have the
 * value of Kbshift returned in bits 24..31. Unfortunately there is no way to
 * recognise input/output redirection or end-of-file. That is why many
 * libraries define the key combination Control-Z (ASCII-code 26) as the
 * character for end-of-file.
 */
int32_t Cconin ( void )
{
	NOT_IMPLEMENTED(GEMDOS, Cconin, 1);
	return TOS_ENOSYS;
}

/**
 * Cconis - 11
 *
 * The GEMDOS routine Cconis checks whether a character is waiting to be read
 * in GEMDOS handle 0 - normally the standard input buffer of con:.
 */
int32_t Cconis ( void )
{
	NOT_IMPLEMENTED(GEMDOS, Cconis, 11);
	return TOS_ENOSYS;
}

/**
 * Cconos - 16
 *
 * The GEMDOS routine Cconos checks whether a character may be output to
 * GEMDOS handle 1 - normally the standard output device con:.
 *
 * Note: The function works correctly with redirection of input/output only
 * as of GEMDOS Version 0.15.
 */
int16_t Cconos ( void )
{
	NOT_IMPLEMENTED(GEMDOS, Cconos, 16);
	return TOS_ENOSYS;
}

/**
 * Cconout - 2
 *
 * The GEMDOS routine Cconout writes the character c to GEMDOS handle 1 -
 * normally the standard output device con: - as a rule the screen.
 *
 * Note: The character c contain the associated ASCII-code in bits 0..7 (all
 * other bits have to be 0). No line-feed translation is done at the output,
 * so ASCII 13 and ACII 10 must be sent to start a new line. The function
 * works correctly with redirection of the input/output only as of GEMDOS
 * Version 0.15; with earlier versions, do not redirect this handle to a
 * write-only device since the call tries to read from the output stream to
 * process the 'special' keys.
 */
int32_t Cconout ( int16_t c )
{
	return write(1, &c, 1);
}

/**
 * Cconrs - 10
 *
 * The GEMDOS routine Cconrs reads a string from the standard input stream
 * (GEMDOS handle 0) - normally the keyboard - and echoes it to the standard
 * output stream (GEMDOS handle 1) - normally the screen.
 *
 * To do this one writes in the component maxlen of LINE the number of bytes
 * to be read in - 1. The input will be terminated by the function when the
 * Return key is pressed or the maximum input length has been exceeded.
 *
 * 'Special' key combinations cause various actions. In detail: Input Meaning
 *
 * int32_t Cconrs ( LINE *buf )
 */
int32_t Cconrs ( emuptr32_t buf )
{
	NOT_IMPLEMENTED(GEMDOS, Cconrs, 10);
	return TOS_ENOSYS;
}

/**
 * Cconws - 9
 *
 * The GEMDOS routine Cconws writes the string buf to GEMDOS handle 1 -
 * normally the standard output device con:. The string must be
 * NULL-terminated (with the ASCII character 0). With GEMDOS versions below
 * 0.15, do not redirect this handle to a write-only device since the call
 * tries to read from the output stream to process the 'special' keys.
 *
 * int32_t Cconws ( CONST BYTE *buf )
 */
int32_t Cconws ( emuptr32_t address )
{
	uint8_t buffer[256];
	int done = 0;
	while(!done)
	{
		int count = 0;
		for(;count < 256;count++)
		{
			buffer[count] = m68k_read_memory_8(address++);
			if (buffer[count] == 0)
			{
				count--;
				done=1;
				break;
			}
		}

		int32_t bytes_written = write(1, buffer, count);
		if (bytes_written < 0)
		{
			return MapErrno();
		}
	}
	return 0;
}

/**
 * Cnecin - 8
 *
 * The GEMDOS routine Cnecin reads a character from GEMDOS handle 0 -
 * normally the standard input device con: (the keyboard as a rule), without
 * outputting it to the standard output device (normally the screen).
 *
 * Note: Flow control can be achieved with the key combinations Control-S
 * (stop output) or Control-Q (continue output). With GEMDOS versions prior
 * to 0.30, if the function's handle is redirected an end-of-file condition
 * will cause the system to hang.
 */
int32_t Cnecin ( void )
{
	NOT_IMPLEMENTED(GEMDOS, Cnecin, 8);
	return TOS_ENOSYS;
}

/**
 * Cprnos - 17
 *
 * The GEMDOS routine Cprnos checks whether handle 3 - normally the parallel
 * port prn: - is ready to accept characters.
 *
 * Note: It is strongly recommended to use this function before starting a
 * printout, since it takes around 30 seconds to recognize a printer timeout
 * if the printer is not ready (perhaps not switched on?).
 */
int16_t Cprnos ( void )
{
	NOT_IMPLEMENTED(GEMDOS, Cprnos, 17);
	return TOS_ENOSYS;
}

/**
 * Cprnout - 5
 *
 * The GEMDOS routine Cprnout writes the character c to GEMDOS handle 3 -
 * normally the printer port prn:.
 *
 * c is a WORD value, with the character to be output occupying bits 0..7;
 * all other bits must be 0.
 *
 * Note: Before print output one should check with Cprnos whether the printer
 * is ready. Settings made with the aid of the Setprt function are ignored
 * for printing by almost all TOS versions. With redirection of input/output,
 * the function works correctly only as of GEMDOS Version 0.15.
 */
int32_t Cprnout ( int16_t c )
{
	NOT_IMPLEMENTED(GEMDOS, Cprnout, 5);
	return TOS_ENOSYS;
}

/**
 * Crawcin - 7
 *
 * The GEMDOS routine Crawcin reads a character from GEMDOS handle 1 -
 * normally the standard input con: (the keyboard) - without echoing it
 * immediately to the screen or processing any of the 'special' keys.
 *
 * Note: The function works correctly with input/output redirection only as
 * of GEMDOS Version 0.15.
 */
int32_t Crawcin ( void )
{
	struct termios tp, save;
	int needRestore = 0;
    char input;

    /* Retrieve current terminal settings, turn echoing off */

    if (tcgetattr(0, &tp) != -1)
    {
		save = tp;                          /* So we can restore settings later */
		cfmakeraw(&tp);						/* Switch termial to raw mode */
	    needRestore = (tcsetattr(0, TCSAFLUSH, &tp) != -1);
	}

	int retval = TOS_ENOSYS;

	if (read(0, &input, 1) == -1)
	{
		retval = MapErrno();
	}
	else
	{
		retval = input;
	}
	if (needRestore)
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &save);
	//printf("Crawcin --> %08x %c\n", retval, retval);

	return retval;
}

/**
 * Crawio - 6
 *
 * The GEMDOS routine Crawio combines unbuffered console input and output in
 * one function. The following applies for the parameter w, of which only the
 * lower 8 bits are significant (others should be 0): Value Meaning
 */
int32_t Crawio ( int16_t w )
{
	if((w & 0xff) == 0xff)
		return Crawcin();
	else
		return Cconout(w);
}
