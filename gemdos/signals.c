#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "common.h"
#include "tos_errors.h"
#include "gemdos.h"

enum mint_signal
{
	MINT_SIGNULL	=  0,		/* not really a signal */
	MINT_SIGHUP		=  1,		/* hangup signal */
	MINT_SIGINT		=  2,		/* sent by ^C */
	MINT_SIGQUIT	=  3,		/* quit signal */
	MINT_SIGILL		=  4,		/* illegal instruction */
	MINT_SIGTRAP	=  5,		/* trace trap */
	MINT_SIGABRT	=  6,		/* abort signal */
	MINT_SIGPRIV	=  7,		/* privilege violation */
	MINT_SIGFPE		=  8,		/* divide by zero */
	MINT_SIGKILL	=  9,		/* cannot be ignored */
	MINT_SIGBUS		= 10,		/* bus error */
	MINT_SIGSEGV	= 11,		/* illegal memory reference */
	MINT_SIGSYS		= 12,		/* bad argument to a system call */
	MINT_SIGPIPE	= 13,		/* broken pipe */
	MINT_SIGALRM	= 14,		/* alarm clock */
	MINT_SIGTERM	= 15,		/* software termination signal */
	MINT_SIGURG		= 16,		/* urgent condition on I/O channel */
	MINT_SIGSTOP	= 17,		/* stop signal not from terminal */
	MINT_SIGTSTP	= 18,		/* stop signal from terminal */
	MINT_SIGCONT	= 19,		/* continue stopped process */
	MINT_SIGCHLD	= 20,		/* child stopped or exited */
	MINT_SIGTTIN	= 21,		/* read by background process */
	MINT_SIGTTOU	= 22,		/* write by background process */
	MINT_SIGIO		= 23,		/* I/O possible on a descriptor */
	MINT_SIGXCPU	= 24,		/* CPU time exhausted */
	MINT_SIGXFSZ	= 25,		/* file size limited exceeded */
	MINT_SIGVTALRM	= 26,		/* virtual timer alarm */
	MINT_SIGPROF	= 27,		/* profiling timer expired */
	MINT_SIGWINCH	= 28,		/* window size changed */
	MINT_SIGUSR1	= 29,		/* user signal 1 */
	MINT_SIGUSR2	= 30,		/* user signal 2 */
	MINT_SIGPWR		= 31		/* power failure (restart) */
};

static inline uint32_t signal_to_mask(enum mint_signal signal)
{
	return 1 << signal;
}

void mask_to_sigset(sigset_t* set, uint32_t mask)
{
	sigemptyset(set);
	if (mask & signal_to_mask(MINT_SIGHUP))
		sigaddset(set, SIGHUP);
	if (mask & signal_to_mask(MINT_SIGINT))
		sigaddset(set, SIGINT);
	if (mask & signal_to_mask(MINT_SIGQUIT))
		sigaddset(set, SIGQUIT);
	if (mask & signal_to_mask(MINT_SIGILL))
		sigaddset(set, SIGILL);
	if (mask & signal_to_mask(MINT_SIGTRAP))
		sigaddset(set, SIGTRAP);
	if (mask & signal_to_mask(MINT_SIGABRT))
		sigaddset(set, SIGABRT);
	//if (mask & signal_to_mask(MINT_SIGPRIV))
	//	sigaddset(set, SIGPRIV);
	if (mask & signal_to_mask(MINT_SIGFPE))
		sigaddset(set, SIGFPE);
	if (mask & signal_to_mask(MINT_SIGKILL))
		sigaddset(set, SIGKILL);
	if (mask & signal_to_mask(MINT_SIGBUS))
		sigaddset(set, SIGBUS);
	if (mask & signal_to_mask(MINT_SIGSEGV))
		sigaddset(set, SIGSEGV);
	if (mask & signal_to_mask(MINT_SIGSYS))
		sigaddset(set, SIGSYS);
	if (mask & signal_to_mask(MINT_SIGPIPE))
		sigaddset(set, SIGPIPE);
	if (mask & signal_to_mask(MINT_SIGALRM))
		sigaddset(set, SIGALRM);
	if (mask & signal_to_mask(MINT_SIGTERM))
		sigaddset(set, SIGTERM);
	if (mask & signal_to_mask(MINT_SIGURG))
		sigaddset(set, SIGURG);
	if (mask & signal_to_mask(MINT_SIGSTOP))
		sigaddset(set, SIGSTOP);
	if (mask & signal_to_mask(MINT_SIGTSTP))
		sigaddset(set, SIGTSTP);
	if (mask & signal_to_mask(MINT_SIGCONT))
		sigaddset(set, SIGCONT);
	if (mask & signal_to_mask(MINT_SIGCHLD))
		sigaddset(set, SIGCHLD);
	if (mask & signal_to_mask(MINT_SIGTTIN))
		sigaddset(set, SIGTTIN);
	if (mask & signal_to_mask(MINT_SIGTTOU))
		sigaddset(set, SIGTTOU);
	if (mask & signal_to_mask(MINT_SIGIO))
		sigaddset(set, SIGIO);
	if (mask & signal_to_mask(MINT_SIGXCPU))
		sigaddset(set, SIGXCPU);
	if (mask & signal_to_mask(MINT_SIGXFSZ))
		sigaddset(set, SIGXFSZ);
	if (mask & signal_to_mask(MINT_SIGVTALRM))
		sigaddset(set, SIGVTALRM);
	if (mask & signal_to_mask(MINT_SIGPROF))
		sigaddset(set, SIGPROF);
	if (mask & signal_to_mask(MINT_SIGWINCH))
		sigaddset(set, SIGWINCH);
	if (mask & signal_to_mask(MINT_SIGUSR1))
		sigaddset(set, SIGUSR1);
	if (mask & signal_to_mask(MINT_SIGUSR2))
		sigaddset(set, SIGUSR2);
#ifdef SIGPWR
	if (mask & signal_to_mask(MINT_SIGPWR))
		sigaddset(set, SIGPWR);
#endif
}

uint32_t sigset_to_mask(sigset_t* set)
{
	uint32_t mask = 0;
	if (sigismember(set, SIGHUP))
		mask |= signal_to_mask(MINT_SIGHUP);
	if (sigismember(set, SIGINT))
		mask |= signal_to_mask(MINT_SIGINT);
	if (sigismember(set, SIGQUIT))
		mask |= signal_to_mask(MINT_SIGQUIT);
	if (sigismember(set, SIGILL))
		mask |= signal_to_mask(MINT_SIGILL);
	if (sigismember(set, SIGTRAP))
		mask |= signal_to_mask(MINT_SIGTRAP);
	if (sigismember(set, SIGABRT))
		mask |= signal_to_mask(MINT_SIGABRT);
	//if (sigismember(set, SIGPRIV))
	//	mask |= signal_to_mask(MINT_SIGPRIV);
	if (sigismember(set, SIGFPE))
		mask |= signal_to_mask(MINT_SIGFPE);
	if (sigismember(set, SIGKILL))
		mask |= signal_to_mask(MINT_SIGKILL);
	if (sigismember(set, SIGBUS))
		mask |= signal_to_mask(MINT_SIGBUS);
	if (sigismember(set, SIGSEGV))
		mask |= signal_to_mask(MINT_SIGSEGV);
	if (sigismember(set, SIGSYS))
		mask |= signal_to_mask(MINT_SIGSYS);
	if (sigismember(set, SIGPIPE))
		mask |= signal_to_mask(MINT_SIGPIPE);
	if (sigismember(set, SIGALRM))
		mask |= signal_to_mask(MINT_SIGALRM);
	if (sigismember(set, SIGTERM))
		mask |= signal_to_mask(MINT_SIGTERM);
	if (sigismember(set, SIGURG))
		mask |= signal_to_mask(MINT_SIGURG);
	if (sigismember(set, SIGSTOP))
		mask |= signal_to_mask(MINT_SIGSTOP);
	if (sigismember(set, SIGTSTP))
		mask |= signal_to_mask(MINT_SIGTSTP);
	if (sigismember(set, SIGCONT))
		mask |= signal_to_mask(MINT_SIGCONT);
	if (sigismember(set, SIGCHLD))
		mask |= signal_to_mask(MINT_SIGCHLD);
	if (sigismember(set, SIGTTIN))
		mask |= signal_to_mask(MINT_SIGTTIN);
	if (sigismember(set, SIGTTOU))
		mask |= signal_to_mask(MINT_SIGTTOU);
	if (sigismember(set, SIGIO))
		mask |= signal_to_mask(MINT_SIGIO);
	if (sigismember(set, SIGXCPU))
		mask |= signal_to_mask(MINT_SIGXCPU);
	if (sigismember(set, SIGXFSZ))
		mask |= signal_to_mask(MINT_SIGXFSZ);
	if (sigismember(set, SIGVTALRM))
		mask |= signal_to_mask(MINT_SIGVTALRM);
	if (sigismember(set, SIGPROF))
		mask |= signal_to_mask(MINT_SIGPROF);
	if (sigismember(set, SIGWINCH))
		mask |= signal_to_mask(MINT_SIGWINCH);
	if (sigismember(set, SIGUSR1))
		mask |= signal_to_mask(MINT_SIGUSR1);
	if (sigismember(set, SIGUSR2))
		mask |= signal_to_mask(MINT_SIGUSR2);
#ifdef SIGPWR
	if (sigismember(set, SIGPWR))
		mask |= signal_to_mask(MINT_SIGPWR);
#endif
	return mask;
}

/**
 * Pause - 289
 *
 * The function Pause suspends the calling process until a signal for this
 * arrives. If a signal-handler has been installed for this signal with
 * Psignal, the handler will be called before the function Pause returns.
 *
 * The function will not return if the signal-handler executes a non-local
 * jump (via longjump), or if the program is terminated.
 */
void Pause ( void )
{
	pause();
}

/**
 * Pkill - 273
 *
 * The function Pkill sends the signal sig to one or more processes. The
 * following apply for the parameter pid: pid Meaning
 */
int16_t Pkill ( int16_t pid, int16_t sig )
{
	NOT_IMPLEMENTED(GEMDOS, Pkill, 273);
	return TOS_ENOSYS;
}

/**
 * Psigaction - 311
 *
 * The function Psigaction alters the response to the signal sig. The
 * parameter act for this is either NULL, or points to a sigaction structure
 * that describes the behaviour of the signal handling. The following applies
 * for the component sa_handler: sa_handler Meaning
 *
 * int32_t Psigaction ( int16_t sig, struct sigaction *act, struct sigaction *oact )
 */
int32_t Psigaction ( int16_t sig, emuptr32_t act, emuptr32_t oact )
{
	NOT_IMPLEMENTED(GEMDOS, Psigaction, 311);
	return TOS_ENOSYS;
}

/**
 * Psigblock - 278
 *
 * The function Psigblock blocks selected signals from delivery. It adds the
 * signals specified in mask to the set of currently blocking signals. For
 * this, each bit of the parameter mask represents one signal. If bit n in
 * mask is set, it means that the signal with the number n will be blocked.
 *
 * One should note that some signals (e.g. SIGKILL) can not be blocked. The
 * kernel will delete these signals from mask before any change of the signal
 * set is performed.
 *
 * Furthermore it should be pointed out that blocked signals also remain
 * blocked via Pfork/Pvfork calls. After a Pexec call the child always starts
 * with an empty set of signals to be blocked, irrespective of which signals
 * were blocked by its parent.
 *
 * Warning: This function is optional, hence a call may be answered with
 * EINVFN.
 */
int32_t Psigblock ( int32_t mask )
{
	sigset_t set, old;
	mask_to_sigset(&set, mask);
	if(sigprocmask(SIG_BLOCK, &set, &old) == -1)
		return MapErrno();
	return sigset_to_mask(&old);
}

/**
 * Psigintr - 318
 *
 * The function Psigintr assigns a signal to a particular exception vector.
 * When the exception occurs, the kernel will send the signal to the process.
 *
 * vec specifies the exception vector. This is the same value as specified
 * for Setexc() call. sig specifies the signal number that is supposed to be
 * delivered when an exception assigned to the vector vec occurs. When both
 * sig and vec are zero, all handlers installed by your program are removed.
 *
 * You should install a signal-handler prior to making this call, otherwise
 * your process will most probably get killed by the first occurrence of the
 * interrupt assigned to vec vector.(!nl) Also notice that the function is
 * not available on machines equipped with 68000 and 68010 processors.
 *
 * This function has been totally rewritten as of MiNT version 1.15.1.
 * However, the only change visible to programs is that the old value of vec
 * is no longer returned (it had little use anyway). Also, since long stack
 * frames are needed, a 68020 or newer processor is required.
 *
 * The handler set up by Psigintr gets removed when your process terminates.
 */
int32_t Psigintr ( int16_t vec, int16_t sig )
{
	NOT_IMPLEMENTED(GEMDOS, Psigintr, 318);
	return TOS_ENOSYS;
}

/**
 * Psignal - 274
 *
 * The function Psignal alters the action to be taken when the signal sig
 * arrives. The parameter handler can assume 3 possible values: handler
 * Meaning
 */
int32_t Psignal ( int16_t sig, int32_t handler )
{
	NOT_IMPLEMENTED(GEMDOS, Psignal, 274);
	return TOS_ENOSYS;
}

/**
 * Psigpause - 310
 *
 * The function Psigpause sets a new signal mask mask, and suspends the
 * called process until a signal arrives that is not masked or ignored.
 *
 * If a handler has been installed for this signal with Psignal, then this
 * will be called before the function returns. If the handler executes a
 * longjump to another part of the program, or the process terminates, then
 * the function will never return.
 *
 * Note: When the function returns, the signal mask will be reset to the
 * value that applied before the call of Psigpause. Thus the signal mask set
 * by the function is only valid temporarily. In MagiC, problems may arise if
 * several threads call this function at the same time, as the signal mask
 * here is process-global.
 */
int32_t Psigpause ( int32_t mask )
{
	sigset_t set;
	mask_to_sigset(&set, mask);
	sigsuspend(&set);
	return MapErrno();
}

/**
 * Psigpending - 291
 *
 * The function Psigpending returns the signals that have been sent to the
 * calling process, but not yet handled (say because they are blocked with
 * Psigblock or because they are currently being processed).
 */
int32_t Psigpending ( void )
{
	sigset_t set;
	if (sigpending(&set) == -1)
		return MapErrno();
	return sigset_to_mask(&set);
}

/**
 * Psigreturn - 282
 *
 * The function Psigreturn prepares for the exit from a signal-handler. As
 * this happens automatically when the handler returns, calling Psigreturn is
 * only necessary if the handler executes a non-local jump (perhaps with
 * longjump) rather than using RTS.
 *
 * The call has no effect when no signal is being processed at the time.
 *
 * In MagiC, the thread of the active signal-handler will become the main
 * thread of the process and then be removed. All other signal processes will
 * be removed as well (nesting). The locked semaphores of the main thread
 * will be released, and the supervisor stack will be reset to the value at
 * process start.
 */
void Psigreturn ( void )
{
	NOT_IMPLEMENTED(GEMDOS, Psigreturn, 282);
}

/**
 * Psigsetmask - 279
 *
 * The function Psigsetmask replaces the set of signals that are currently to
 * be blocked completely by the signals specified in the parameter mask.
 *
 * One should note that some signals (e.g. SIGKILL) can not be blocked. The
 * kernel will delete these signals from mask before any change of the signal
 * set is performed.
 *
 * Furthermore it should be pointed out that blocked signals also remain
 * blocked via Pfork/Pvfork calls. After a Pexec call the child always starts
 * with an empty set of signals to be blocked, irrespective of which signals
 * were blocked by its parent.
 */
int32_t Psigsetmask ( int32_t mask )
{
	sigset_t set, old;
	mask_to_sigset(&set, mask);
	if(sigprocmask(SIG_SETMASK, &set, &old) == -1)
		return MapErrno();
	return sigset_to_mask(&old);
}
