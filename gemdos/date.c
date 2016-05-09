#include <time.h>
#include <sys/time.h>
#include "common.h"
#include "tos_errors.h"

/**
 * Talarm - 288
 *
 * The function Talarm sends a SIGALRM signal to the calling process after
 * time seconds.
 *
 * If no signal-handler was installed for this signal with Psignal, then the
 * process will be terminated immediately (i.e. on receiving the signal).
 *
 * By setting the value of time = 0, an earlier set alarm can be cancelled.
 * If time is negative, then the function has no effect and only the return
 * value is delivered.
 */
int32_t Talarm ( int32_t time )
{
	NOT_IMPLEMENTED(GDOS, Talarm, 288);
	return TOS_ENOSYS;
}

/**
 * Tgetdate - 42
 *
 * The GEMDOS routine Tgetdate obtains the current date.
 */
uint32_t Tgetdate ( void )
{
	time_t current=time(NULL);
	struct tm* t = localtime(&current);
	return t->tm_mday | ((t->tm_mon+1)<<5) | (((t->tm_year-80)&0x7f)<<9);
}

/**
 * Tgettime - 44
 *
 * The GEMDOS routine Tgettime obtains the system time.
 */
uint32_t Tgettime ( void )
{
	time_t current=time(NULL);
	struct tm* t = localtime(&current);
	return ((t->tm_sec/2)&0xF) | (t->tm_min << 5) | (t->tm_hour << 11);
}

/**
 * Tgettimeofday - 341
 *
 * The function Tgettimeofday interrogates the state of the internal, high
 * resolution system clock.
 *
 * The argument tv is a pointer to the timeval structure.
 *
 * The argument tzp is a pointer to timezone structure.
 *
 * You may safely pass NULL for either argument. This isn't considered an
 * error.
 *
 * Implementors of library bindings should be aware that the definition of
 * struct timezone is non-standard. The members are actually int and not long
 * int (this applies only to struct timezone; the members of struct timeval
 * are always long). 16-bit libraries will have to copy the contents of the
 * structure that tzp points to.
 *
 * The Ssystem() call has a command S_CLOCKMODE. This command allows to
 * retrieve or set the kernel clock mode, i.e. to specify whether the
 * hardware clock is meant to run in UTC or in local time.
 * It is planned to make MiNT compliant with the kernel time keeping model
 * described in RFC1305. This model is already successfully implemented in
 * operating systems such as SunOS, Ultrix, OSF/1, HP-UX and Linux. Please
 * expect the internal realization to change in the future.
 *
 * int32_t Tgettimeofday ( timeval *tv, timezone *tzp )
 */
int32_t Tgettimeofday ( emuptr32_t tv, emuptr32_t tzp )
{
	struct timeval timeval;
	struct timezone timezone;
	if( 0 == gettimeofday(&timeval, &timezone))
	{
		if( tv )
		{
			m68k_write_memory_32(tv, timeval.tv_sec);
			m68k_write_memory_32(tv+4, timeval.tv_usec);
		}
		if ( tzp )
		{
			m68k_write_memory_32(tzp, timezone.tz_minuteswest);
			m68k_write_memory_32(tzp+4, timezone.tz_dsttime);
		}
	}
	return TOS_E_OK;
}

/**
 * Tmalarm - 317
 *
 * This function reads/sets a process alarm for the current process.
 *
 * time specifies the length of time (in milliseconds) to wait before a
 * SIGALRM signal is delivered. If time is 0 then any previously set alarm is
 * cancelled. If time is negative the function does not modify any alarm
 * currently set.
 *
 * If no SIGALRM signal handler has been set up when the alarm is triggered,
 * the process will be killed.
 */
int32_t Tmalarm( int32_t time )
{
	NOT_IMPLEMENTED(GDOS, Tmalarm, 317);
	return TOS_ENOSYS;
}

/**
 * Tsetdate - 43
 *
 * The GEMDOS routine Tsetdate sets the current date. The parameter date is
 * coded as follows: Bits Meaning
 */
int16_t Tsetdate ( uint16_t date )
{
	NOT_IMPLEMENTED(GDOS, Tsetdate, 43);
	return TOS_ENOSYS;
}

/**
 * Tsetitimer - 329
 *
 * The GEMDOS routine Tsetitimer controls one of three possible periodic
 * timers. The following apply: Parameter Meaning
 *
 * int32_t Tsetitimer ( int16_t which, int32_t *interval, int32_t *value, int32_t *ointerval, int32_t *ovalue )
 */
int32_t Tsetitimer ( int16_t which, emuptr32_t interval, emuptr32_t value, emuptr32_t ointerval, emuptr32_t ovalue )
{
	NOT_IMPLEMENTED(GDOS, Tsetitimer, 329);
	return TOS_ENOSYS;
}

/**
 * Tsettime - 45
 *
 * The GEMDOS routine Tsettime sets the time. The parameter time must be
 * coded as follows: Bits Meaning
 */
int16_t Tsettime ( uint16_t time )
{
	NOT_IMPLEMENTED(GDOS, Tsettime, 45);
	return TOS_ENOSYS;
}

/**
 * Tsettimeofday - 342
 *
 * The function Tsettimeofday sets the state of the internal, high resolution
 * system clock.
 *
 * The argument tv is a pointer to the timeval structure.
 *
 * The argument tzp is a pointer to the timezone structure.
 *
 * You may safely pass NULL for either argument. This isn't considered an
 * error.
 *
 * Implementors of library bindings should be aware that the definition of
 * struct timezone is non-standard. The members are actually int and not long
 * int (this applies only to struct timezone; the members of struct timeval
 * are always long). 16-bit libraries will have to copy the contents of the
 * structure that tzp points to.
 *
 * The tz_dsttime member of timezone is stored, but not evaluated within the
 * kernel. Beware not to misunderstand its meaning: if non-zero it simply
 * signifies that daylight savings time applies during some part of the year,
 * not necessarily now. In other words: if it is non-zero someday, it should
 * be non-zero during the entire year. The Ssystem call has a command
 * S_CLOCKMODE. This command allows retrieval or setting of the kernel clock
 * mode, i.e. to specify whether the hardware clock is meant to run in UTC or
 * in local time.(!nl) It is planned to make MiNT compliant with the kernel
 * timekeeping model described in RFC1305. This model is already successfully
 * implemented in operating systems such as SunOS, Ultrix, OSF/1, HP-UX and
 * Linux. Please expect the internal realization to change in the future.
 *
 * int32_t Tsettimeofday ( timeval *tv, timezone *tzp )
 */
int32_t Tsettimeofday ( emuptr32_t tv, emuptr32_t tzp )
{
	NOT_IMPLEMENTED(GDOS, Tsettimeofday, 342);
	return TOS_ENOSYS;
}
