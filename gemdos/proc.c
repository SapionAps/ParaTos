#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>

#include "common.h"
#include "tos_errors.h"
#include "gemdos.h"

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
 * Pfork - 283
 *
 * The function Pfork creates a copy of the current process. The child
 * process inherits a copy of the address space of the parent process, i.e.
 * changes made to variables by the child have no effect on the parent.
 *
 * The newly created process starts its work when the function Pfork returns.
 *
 * If the parent is in supervisor-mode when this call is made, the child is
 * launched in user-mode anyway.
 *
 * New processes launched with this call should not use Mshrink, but are
 * required to do any GEM initialisation such as appl_init and v_openvwk
 * again (if they need to use GEM). Both parent and child use Pterm or Pterm0
 * to terminate themselves.
 */
int16_t Pfork ( void )
{
	return fork();
}

/**
 * Pgetauid - 326
 *
 * The function Pgetauid obtains the start user ID of the current process.
 * Together with Psetauid one can use it in a multi-user environment to
 * ascertain who was the first logged-in user.
 */
int16_t Pgetauid ( void )
{
	NOT_IMPLEMENTED(GEMDOS, Pgetauid, 326);
	return TOS_ENOSYS;
}

/**
 * Pgetegid - 313
 *
 * The function Pgetegid returns the effective group ID of the current
 * process and resembles the system call Pgetgid. The difference lies in that
 * here the effective group ID is returned.
 *
 * This is normally identical to the actual group ID; however, if a program
 * is running that has set the setgid bit, its effective group ID is equal to
 * the (actual) ID of the owner of this program file.
 *
 * File accesses are based, among other things, on the effective group ID, so
 * that the setgid mechanism allows users (particularly the super-user) to
 * permit access by other users.
 */
int32_t Pgetegid ( void )
{
	return getegid();
}

/**
 * Pgeteuid - 312
 *
 * The function Pgeteuid returns the effective user ID of the current process
 * and resembles the system call Pgetuid. The difference lies in that here
 * the effective user ID is returned.
 *
 * This is normally identical to the actual user ID; however, if a program is
 * running that has set the setuid bit, its effective user ID is equal to the
 * (actual) ID of the owner of this program file.
 *
 * File accesses are based, among other things, on the effective user ID, so
 * that the setuid mechanism allows users (particularly the super-user) to
 * permit access by other users.
 */
int32_t Pgeteuid ( void )
{
	return geteuid();
}

/**
 * Pgetgid - 276
 *
 * The function Pgetgid returns the actual group ID of the current calling
 * processes. This is as number between 0 and 255, which sets the rights of
 * the process group.
 */
int16_t Pgetgid ( void )
{
	return getgid();
}

/**
 * Pgetgroups - 327
 *
 * The function Pgetgroups obtains the supplementary group IDs for the
 * calling process.
 *
 * len specifies the length (in words) of the array to be filled with the
 * supplementary group IDs available. This value must be a positive number
 * not greater than NGROUPS_MAX. If len is a zero, the call will return the
 * number of supplementary group ids currently available for the calling
 * process.
 *
 * gidset is a pointer to a word array to be filled with retrieved ids.
 *
 * The NGROUPS_MAX value is returned by Sysconf().
 *
 * int32_t Pgetgroups( int16_t len, int16_t *gidset)
 */
int32_t Pgetgroups( int16_t len, emuptr32_t gidset)
{
	NOT_IMPLEMENTED(GEMDOS, Pgetgroups, 327);
	return TOS_ENOSYS;
}

/**
 * Pgetpgrp - 269
 *
 * The function Pgetpgrp returns the group number code of the currently
 * active process. Process groups are normally used for job-control and
 * similar purposes.
 */
int16_t Pgetpgrp ( void )
{
	return getpgrp();
}

/**
 * Pgetpid - 267
 *
 * The function Pgetpid returns the PID of the currently active (calling)
 * process, i.e. the one that currently 'owns' the CPU; this is the filetype
 * under which the current process is administered in the directory U:\PROC.
 *
 * A PID is dynamically assigned to a process when it is created. You should
 * make no assumption on the PID a process may have. For the kernel, the PID
 * is a WORD but when you program in C, you should use the pid_t type, which
 * complies with the POSIX standard. The FreeMiNT kernel's PID is always 0.
 *
 * The PID is useful when it is important to identify a process among others
 * in the system. Knowing the PID of a program, it is possible to do a lot of
 * things: send it a signal with Psignal(), look for its name in /proc by
 * searching with a "*.xxx" mask (xxx is the PID of the program which name we
 * wanna know), etc.
 */
int16_t Pgetpid ( void )
{
	return getpid();
}

/**
 * Pgetppid - 268
 *
 * The function Pgetppid returns the PID of the parent process of the
 * currently active (calling) process.
 *
 * In the FreeMiNT kernel, processes are represented as a tree. The root of
 * the tree is the kernel itself (PID 0). At boot time, the kernel launches
 * another process (which can be 'init' in case of a Unix-like setup, or an
 * AES, a shell or whatever) specified by the INIT or GEM keyword in the
 * mint.cnf file. This process will launch other processes (daemons,
 * accessories, programs etc) that are its so-called children.
 *
 * Therefore, every process (but the kernel, obviously) is created by its
 * so-called parent. The Pgetppid() (get parent's PID) call allows
 * determination the PID of the parent of the calling process. To have a good
 * illustration of this, you can use the 'ps' command in your shell. You will
 * see that all accessories parent is the AES, and that the daemons are
 * started by the 'init' program.
 */
int16_t Pgetppid ( void )
{
	return getppid();
}

/**
 * Pgetpriority - 344
 *
 * The function Pgetpriority obtains the priority of the processes specified
 * by which and who. The interpretation of parameter who depends on which:
 *
 * PRIO_PROCESS (0) reads the priority of process with process ID which. A
 * who of 0 implies the process id of the calling process.
 *
 * PRIO_PGRP (1) reads the priority of the process group with process group
 * ID who. If the priorities of the process differ, the lowest valued
 * priority (i.e. the highest CPU usage priority) is returned. A who of 0
 * implies the process group id of the calling process.
 *
 * PRIO_USER (2) reads the priority of the process of the user with user ID
 * who. For multiple processes the lowest valued priority is returned. A who
 * of 0 implies the user id of the calling process.
 *
 * Library functions should first check for an error condition and then
 * decrement the returned value by 20.
 *
 * This call makes calls Pnice and Prenice obsolete.
 */
int32_t Pgetpriority ( int16_t which, int16_t who)
{
	NOT_IMPLEMENTED(GEMDOS, Pgetpriority, 344);
	return TOS_ENOSYS;
}

/**
 * Pgetuid - 271
 *
 * The function Pgetuid returns the actual user ID of the current (calling)
 * process. This is a number between 0 and 255 that determines the access
 * rights permissions of the process, and can be used to differentiate the
 * individual users in multi-user systems.
 */
int16_t Pgetuid ( void )
{
	return getuid();
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
 * Pmsg - 293
 *
 * The function Pmsg sends or receives a message to or from a 'mail box'. The
 * operation to be executed in each case can be set with the bits of the
 * parameter mode. The following apply: mode Operation
 *
 * int32_t Pmsg ( int16_t mode, int32_t mbox, void *msg )
 */
int32_t Pmsg ( int16_t mode, int32_t mbox, emuptr32_t msg )
{
	NOT_IMPLEMENTED(GEMDOS, Pmsg, 293);
	return TOS_ENOSYS;
}

/**
 * Pnice - 266
 *
 * The function Pnice alters the priority of the process calling it by the
 * value delta. A higher priority means that the process receives a larger
 * time-slice, i.e. it is not switched over so often. In the same way, a
 * lower priority means that the time-slice of the process is reduced, and so
 * it must yield time to the CPU more often.
 *
 * For the parameter delta the following must apply:(!nl) - 20 Prenice
 * (Pgetpid, delta) and is implemented for backwards compatibility.
 */
int16_t Pnice ( int16_t delta )
{
	NOT_IMPLEMENTED(GEMDOS, Pnice, 266);
	return TOS_ENOSYS;
}

/**
 * Prenice - 295
 *
 * The function Prenice alters the priority of the process with the ID pid by
 * the value delta. A higher priority means that the process receives a
 * larger time-slice, i.e. it is not switched over so often. In the same way,
 * a lower priority means that the time-slice of the process is reduced, and
 * so it must yield time to the CPU more often.
 *
 * For the parameter delta the following must apply:(!nl) - 20
 */
int32_t Prenice ( int16_t pid, int16_t delta )
{
	NOT_IMPLEMENTED(GEMDOS, Prenice, 295);
	return TOS_ENOSYS;
}

/**
 * Prusage - 286
 *
 * The function Prusage returns information about the system resources
 * required by the current process.
 *
 * The information is passed to the outside by a pointer r to an array of 8
 * LONGs; the allocation is as follows: r[0]: Process time spent in MiNT
 * kernel
 *
 * void Prusage ( int32_t *r )
 */
void Prusage ( emuptr32_t r )
{
	NOT_IMPLEMENTED(GEMDOS, Prusage, 286);
}

/**
 * Psemaphore - 308
 *
 * The function Psemaphore implements non-counting semaphores. Only one
 * process can access a semaphore at the same time.
 *
 * Semaphores can be used, for instance, to synchronise accesses to data
 * structures in shared memory by different subprocesses: Hence a subprocess
 * must try, before it accesses memory, to obtain possession of the
 * semaphore. This can be created during initialisation, for instance, and
 * removed again at program termination.
 *
 * Semaphores are identified by a name. This name is a LONGword that may
 * contain four ASCII characters. Semaphores whose name starts with an
 * underscore '_' are reserved for the operating system.
 *
 * The parameter timeout is only used in mode 2. The following assignments
 * apply: timeout = 0: Return immediately.
 */
int32_t Psemaphore ( int16_t mode, int32_t id, int32_t timeout )
{
	NOT_IMPLEMENTED(GEMDOS, Psemaphore, 308);
	return TOS_ENOSYS;
}

/**
 * Psetauid - 325
 *
 * The function Psetauid sets the starting user ID of the current process.
 * This can only happen once. Together with Pgetauid one can ascertian who
 * was the first logged-in user in a multi-user environment. The parameter id
 * is the desired start ID.
 *
 * Warning: This function is optional, hence a call may be answered with
 * EINVFN.
 */
int16_t Psetauid ( int16_t id )
{
	NOT_IMPLEMENTED(GEMDOS, Psetauid, 325);
	return TOS_ENOSYS;
}

/**
 * Psetegid - 324
 *
 * egid specifies the new effective group ID for the calling process.
 *
 * Prior to MiNT version 1.12, this call contained a bug affecting its
 * functionality. Setting effective group id needs root privileges.
 *
 * This call is often used by daemon processes to downgrade their privileges
 * to user level.
 */
int32_t Psetegid ( int16_t egid )
{
	NOT_IMPLEMENTED(GEMDOS, Psetegid, 324);
	return TOS_ENOSYS;
}

/**
 * Pseteuid - 323
 *
 * euid specifies the new effective user ID for the calling process.
 *
 * Prior to MiNT version 1.12 this call contained a bug affecting its
 * functionality. Setting effective user ID needs root privileges.
 *
 * This call is often used by daemon processes to downgrade their privileges
 * to user level.
 */
int32_t Pseteuid ( int16_t euid )
{
	NOT_IMPLEMENTED(GEMDOS, Pseteuid, 323);
	return TOS_ENOSYS;
}

/**
 * Psetgid - 277
 *
 * The function Psetgid sets the group ID of the current process to id. This
 * must be a number between 0 and 255.
 */
int16_t Psetgid ( int16_t id )
{
	NOT_IMPLEMENTED(GEMDOS, Psetgid, 277);
	return TOS_ENOSYS;
}

/**
 * Psetgroups - 328
 *
 * The function Psetgroups sets the supplementary group IDs for the calling
 * process.
 *
 * len specifies the length (in words) of the array containing the
 * supplementary group IDs to be set. This value must be a positive number
 * not greater than NGROUPS_MAX. If len is a zero, the call will return the
 * number of supplementary group ids those have been set.
 *
 * gidset is a pointer to a word array containing the new supplementary group
 * IDs for the process.
 *
 * Setting new supplementary group IDs needs root privileges.
 *
 * The NGROUPS_MAX value is returned by Sysconf().
 *
 * int32_t Psetgroups( int16_t len, int16_t *gidset)
 */
int32_t Psetgroups( int16_t len, emuptr32_t gidset)
{
	NOT_IMPLEMENTED(GEMDOS, Psetgroups, 328);
	return TOS_ENOSYS;
}

/**
 * Psetlimit - 287
 *
 * The function Psetlimit serves for setting or obtaining resource allocation
 * limits for the current process. The parameter lim determines the desired
 * resource type: lim Meaning
 */
int32_t Psetlimit ( int16_t lim, int32_t value )
{
	NOT_IMPLEMENTED(GEMDOS, Psetlimit, 287);
	return TOS_ENOSYS;
}

/**
 * Psetpgrp - 270
 *
 * The function Psetpgrp alters the group number of the process with the ID
 * pid to the value newgrp. The process must have the same UID as the current
 * process, or be the parent of it.
 *
 * If pid = 0, the process group of the current process will be set. If
 * newgrp = 0, the process group will be set to the value of the PID of the
 * current process.
 */
int16_t Psetpgrp ( int16_t pid, int16_t newgrp )
{
	NOT_IMPLEMENTED(GEMDOS, Psetpgrp, 270);
	return TOS_ENOSYS;
}

/**
 * Psetpriority - 345
 *
 * The function Psetpriority sets the priority pri (not an increment but an
 * absolute value) for the processes specified by which and who. The
 * interpretation of parameter who depends on which:
 *
 * PRIO_PROCESS (0) sets the priority of process with process ID which. A who
 * of 0 implies the process ID of the calling process.
 *
 * PRIO_PGRP (1) sets the priority of the process group with process group ID
 * who. A who of 0 implies the process group ID of the calling process.
 *
 * PRIO_USER (2) sets the priority of the process of the user with user ID
 * who. A who of 0 implies the user ID of the calling process.
 *
 * The pri argument is silently changed to the maximum (resp. minimum)
 * possible value if it is not in the range between PRIO_MIN (-20) and
 * PRIO_MAX (+20).
 *
 * This call makes calls Pnice and Prenice obsolete.
 */
int32_t Psetpriority ( int16_t which, int16_t who, int16_t pri )
{
	NOT_IMPLEMENTED(GEMDOS, Psetpriority, 345);
	return TOS_ENOSYS;
}

/**
 * Psetregid - 335
 *
 * The function Psetregid sets the real and/or effective group ID for the
 * calling process.
 *
 * rgid specifies the new real group ID and egid specifies the new effective
 * group ID for the calling process. If either argument is -1, only the other
 * gets changed
 *
 * Setting group IDs needs root privileges.
 *
 * This call is often used by daemon processes to downgrade their privileges
 * to user level.
 */
int32_t Psetregid ( int16_t rgid, int16_t egid)
{
	NOT_IMPLEMENTED(GEMDOS, Psetregid, 335);
	return TOS_ENOSYS;
}

/**
 * Psetreuid - 334
 *
 * The function Psetreuid sets the real and/or effective user ID for the
 * calling process.
 *
 * ruid specifies the new real user ID and euid specifies the new effective
 * user ID for the calling process. If either argument is -1, only the other
 * gets changed.
 *
 * Setting user ids needs root privileges.
 *
 * This call is often used by daemon processes to downgrade their privileges
 * to user level.
 */
int32_t Psetreuid ( int16_t ruid, int16_t euid)
{
	NOT_IMPLEMENTED(GEMDOS, Psetreuid, 334);
	return TOS_ENOSYS;
}

/**
 * Psetuid - 272
 *
 * The function Psetuid sets the user ID of the current process to id. This
 * must be a number between 0 and 255. The function fails if the user ID of
 * the process does not have the value 0, i.e. once set, the user ID can no
 * longer be altered.
 */
int16_t Psetuid ( int16_t id )
{
	NOT_IMPLEMENTED(GEMDOS, Psetuid, 272);
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
	NOT_IMPLEMENTED(GEMDOS, Psigblock, 278);
	return TOS_ENOSYS;
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
void Psigpause ( int32_t mask )
{
	NOT_IMPLEMENTED(GEMDOS, Psigpause, 310);
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
	NOT_IMPLEMENTED(GEMDOS, Psigpending, 291);
	return TOS_ENOSYS;
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
	NOT_IMPLEMENTED(GEMDOS, Psigsetmask, 279);
	return TOS_ENOSYS;
}

/**
 * Pterm - 76
 *
 * The GEMDOS routine Pterm terminates a running process, and removes it from
 * memory. The following apply: Parameter Meaning
 */
void Pterm ( uint16_t retcode )
{
	exit(retcode);
}

/**
 * Pterm0 - 0
 *
 * The GEMDOS routine Pterm0 terminates a TOS program and does not return
 * again. The routine returns an exit code of 0.
 *
 * Note: In MagiC this function is executed directly as Pterm(0).
 */
void Pterm0 ( void )
{
	exit(0);
}

/**
 * Ptermres - 49
 *
 * The GEMDOS routine Ptermres terminates a program, yet leaves a part of the
 * program's TPA intact and removes the memory left from GEMDOS's memory
 * list. The following apply: Parameter Meaning
 */
void Ptermres ( int32_t keepcnt, int16_t retcode )
{
	NOT_IMPLEMENTED(GEMDOS, Ptermres, 49);
	exit(retcode);
}

/**
 * Ptrace - 265
 *
 * Ptrace provides tracing and debugging facilities. It allows one process
 * (the tracing process) to control another (the traced process). Most of the
 * time, the traced process runs normally, but when it receives a signal (see
 * sigaction(2)), it stops. The tracing process is expected to notice this
 * via wait(2) or the delivery of a SIGCHLD signal, examine the state of the
 * stopped process, and cause it to terminate or continue as appropriate.
 * Ptrace is the mechanism by which all this happens.
 *
 * The request argument specifies what operation is being performed; the
 * meaning of the rest of the arguments depends on the operation, but except
 * for one special case noted below, all ptrace calls are made by the tracing
 * process, and the pid argument specifies the process ID of the traced
 * process. request can be: PT_TRACE_ME (0) This request is the only one used
 * by the traced process; it declares that the process expects to be traced
 * by its parent. All the other arguments are ignored. (If the parent process
 * does not expect to trace the child, it will probably be rather confused by
 * the results; once the traced process stops, it cannot be made to continue
 * except via ptrace.) When a process has used this request and calls
 * execve(2) or any of the routines built on it (such as execv(3)), it will
 * stop before executing the first instruction of the new image. Also, any
 * setuid or setgid bits on the executable being executed will be ignored.
 *
 * int16_t Ptrace(int16_t request, int16_t pid, void * addr, int32_t data)
 */
int16_t Ptrace(int16_t request, int16_t pid, emuptr32_t  addr, int32_t data)
{
	NOT_IMPLEMENTED(GEMDOS, Ptrace, 265);
	return TOS_ENOSYS;
}

/**
 * Pumask - 307
 *
 * The function Pumask alters the file/directory creation mask of the current
 * process to mode, which is a WORD bitmask of various access permission
 * flags as defined in Fchmod. The new value of the mask is inherited by
 * child processes.
 *
 * When a new file or a new directory is created (with Fcreate or Dcreate
 * respectively), the access rights are normally set so that full access is
 * permitted. (Exception: New files are normally not executable.)
 *
 * With the aid of Pumask one can specify which access rights during creation
 * of a file are not (!) to be permitted.
 *
 * Explicit Fchmod calls are not affected by Pumask.
 *
 * Warning: In MagiC however the mask is not evaluated by any internal XFS of
 * MagiC, and finds use exclusively in UNIX-like file-systems.
 */
int16_t Pumask ( int16_t mode )
{
	return umask(mode);
}

/**
 * Pusrval - 280
 *
 * The function Pusrval permits the setting or interrogation of
 * process-specific information. These are stored in a LONGword, and
 * inherited by all child processes.
 *
 * The meaning of the value can be freely determined by the calling program.
 */
int32_t Pusrval ( int32_t val )
{
	NOT_IMPLEMENTED(GEMDOS, Pusrval, 280);
	return TOS_ENOSYS;
}

/**
 * Pvfork - 275
 *
 * TODO: currently implemented as Pfork.
 * If using vfork, cpu emulation state change need to be isolated from the parent process.
 */
int16_t Pvfork ( void )
{
	return Pfork();
}

/**
 * Pwait - 265
 *
 * The function Pwait is equivalent to Pwait3 (2, NULL) and is offered for
 * reasons of backwards compatibility.
 */
int32_t Pwait ( void )
{
	return Pwaitpid(-1, 2, 0);
}

/**
 * Pwait3 - 284
 *
 * The function Pwait3 is equivalent to a call of Pwaitpid (-1, flag, rusage)
 * and determines with it the exit code as well as the CPU load of a
 * terminated or stopped child process.
 *
 */
int32_t Pwait3 ( int16_t flag, emuptr32_t rusage )
{
	return Pwaitpid(-1, flag, rusage);
}

/**
 * Pwaitpid - 314
 *
 * The function Pwaitpid tries to obtain the exit code of child processes
 * that have been stopped or terminated. If bit 1 of the parameter flag is
 * set, then the child processes currently stopped will be reported; if
 * clear, child processes newly terminated or stopped by tracing will be
 * reported.
 *
 * A stopped process will be reported once, as a rule, (as long as it has not
 * been restarted and stopped again). Equally, a terminated process will be
 * reported only once.
 *
 * If child processes are present that have neither been stopped nor
 * terminated (or are waiting for such an event), then the further behaviour
 * of the function is determined by bit 0 of the parameter flag. The
 * following applies here: Bit-0 = 0: Wait until child is stopped or
 * terminated
 *
 * int32_t Pwaitpid ( int16_t pid, int16_t flag, int32_t *rusage )
 */
int32_t Pwaitpid ( int16_t pid, int16_t flag, emuptr32_t rusagep )
{
	struct rusage usage;
	int status;
	int options = 0;
	if (flag & 1)
		options |= WNOHANG;
	if (flag & 2)
		options |= WUNTRACED;

	pid_t childpid = wait4(pid, &status, options, rusagep?&usage:NULL);
	if (childpid == -1)
	{
		return MapErrno();
	}

	if (rusagep)
	{
		m68k_write_memory_32(rusagep, (uint32_t)(usage.ru_utime.tv_sec*1000 + usage.ru_utime.tv_usec));
		m68k_write_memory_32(rusagep+1, (uint32_t)(usage.ru_stime.tv_sec*1000 + usage.ru_stime.tv_usec));
	}

	uint32_t retval = (uint32_t)childpid << 16;
	uint32_t signum = 0;
	uint32_t code = 0;
	if (WIFEXITED(status))
	{
		code = WEXITSTATUS(status);
	}
	else if (WIFSIGNALED(status))
	{
		signum = WTERMSIG(status);
		if (WCOREDUMP(status))
		{
			retval |= 0x8000;
		}
	}
	else if (WIFSTOPPED(status))
	{
		signum = WSTOPSIG(status);
		code = 0x1f;
	}

	return (int32_t)(retval | ((signum << 8) & 0x7f00) | (code & 0xff));
}
