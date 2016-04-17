#include "common.h"
#include "tos_errors.h"

/**
 * Fattrib - 67
 *
 * The GEMDOS routine Fattrib reads or sets the attributes of a GEMDOS file.
 * Parameter Meaning
 *
 * int16_t Fattrib ( const int8_t *filename, int16_t wflag, int16_t attrib )
 */
int16_t Fattrib ( emuptr32_t filename, int16_t wflag, int16_t attrib )
{
	NOT_IMPLEMENTED(GDOS, Fattrib, 67);
	return -TOS_ENOSYS;
}

/**
 * Fchmod - 306
 *
 * The function alters the access rights for the GEMDOS file name. The
 * parameter mode contains the new access rights:
 * 
 * Access by mode Meaning 
 *
 * int32_t Fchmod ( int8_t *name, int16_t mode )
 */
int32_t Fchmod ( emuptr32_t name, int16_t mode )
{
	NOT_IMPLEMENTED(GDOS, Fchmod, 306);
	return -TOS_ENOSYS;
}

/**
 * Fchown - 305
 *
 * The function alters the user/group ID of the file name, which control the
 * access rights of the file.
 * 
 * This call can only be issued by a process that has an effective user
 * identifier (EUID) which is 0, or which corresponds to the UID of the file;
 * in the latter case the new UID must correspond to the old one, and in
 * addition the calling process must be a member of the group with the group
 * number GID.
 *
 * int32_t Fchown ( int8_t *name, int16_t uid, int16_t gid )
 */
int32_t Fchown ( emuptr32_t name, int16_t uid, int16_t gid )
{
	NOT_IMPLEMENTED(GDOS, Fchown, 305);
	return -TOS_ENOSYS;
}

/**
 * Fclose - 62
 *
 * The GEMDOS routine Fclose closes a file with the file ID handle.
 * 
 * Note: In a network the active locking is lifted. During closing of
 * standard channels, from GEMDOS Version 0.15 onwards the original channel
 * will be reinstated.
 */
int16_t Fclose ( int16_t handle )
{
	NOT_IMPLEMENTED(GDOS, Fclose, 62);
	return -TOS_ENOSYS;
}

/**
 * Fcntl - 260
 *
 * The function performs various command operations on the file with the
 * handle fh. The parameter cmd describes the desired operation, and can
 * assume the following values. (arg here depends on the selected command):
 * 
 * F_DUPFD (0):
 * Duplicates the specified file handle. The following applies for the new
 * interval: arg ENHNDL will be returned. A call of Fdup (handle) is
 * therefore equivalent to Fcntl (handle, 6L, F_DUPFD).
 * 
 * F_GETFD (1):
 * This command returns the non-inheritance flag for the handle fh. The flag
 * is 0 when child processes started with Pexec are to inherit the file
 * handle, otherwise it has the value 1 and the parameter arg is ignored.
 * 
 * F_SETFD (2):
 * This command sets the non-inheritance flag for the handle fh. arg
 * specifies if child processes started with Pexec will inherit the file
 * handle; a value of 1 indicates they will not, a value of 0 that they will.
 * The default value for the flag is 0 for the standard GEMDOS handles in the
 * region 0...5 and 1 for all others, i.e. non-standard handles.
 * 
 * F_GETFL (3):
 * This command returns the file descriptor flags that can be altered by the
 * user. We are here dealing with the parameter mode specified at Fopen, as
 * long as this has not been altered by another call of Fcntl. arg is
 * ignored.
 * 
 * F_SETFL (4):
 * This command sets the file descriptor flags that can be altered by the
 * user to arg (the other bits are ignored, and should be set to the value
 * 0). It is not possible to alter the read/write mode or the modes for file
 * sharing.
 * 
 * F_GETLK (5):
 * The parameter arg is a pointer to an flock structure, in which information
 * about file locks are stored. If a lock exists then it will be copied into
 * the structure. During this the element l_pid will hold the PID of the
 * locking process. If no lock exists, then the element l_type will be set to
 * the value F_UNLCK. For the case that the lock is held by another computer
 * in a network, the element l_pid is set to a value defined by the NFS
 * (Network File System). For this value, 0x1000 l_pid
 * 
 * F_SETLK (6):
 * This command sets (l_type = F_RDLCK or F_WRLCK) or lifts (l_type =
 * F_UNLCK) an advisory lock on the specified file. If this is a FIFO queue
 * (First-In-First-Out), the whole file must be processed at the same time.
 * For the case when the lock conflicts with locks set by other processes,
 * the value ELOCKED will be returned. If one tries to lift a non-existent
 * lock, the value ENSLOCK will be returned. One should note that read-locks
 * may overlap, but write-locks may not. If a locked file is closed, or the
 * corresponding process terminates, then all set locks will be lifted
 * automatically (i.e. reset).
 * 
 * F_SETLKW (7):
 * Similar to F_SETLK, with the difference that if the lock conflicts with
 * those from other processes, then ELOCKED is not returned but the locked
 * process is suspended until the locking from the other process is lifted.
 * 
 * FSTAT (0x4600):
 * Gets the extended attributes of a file. The parameter arg returns a
 * pointer to an XATTR structure that is filled in with the file's extended
 * attributes, as if an Fxattr call had been made. This command is supported
 * in MagiC as of Version 3.0.
 * 
 * FIONREAD (0x4601):
 * In the parameter arg a pointer to a LONG value is returned which shows the
 * number of bytes that may be read currently from a specified file handle
 * without causing the process to block (wait for more input). The returned
 * value is not necessarily correct, as the exact amount can only be
 * estimated in some cases. The command is supported in MagiC as of Version
 * 3.0.
 * 
 * FIONWRITE (0x4602):
 * Similar to FIONREAD, only here the number of bytes that may be written at
 * present is returned. The command is supported in MagiC as of Version 3.0.
 * 
 * FUTIME (0x4603):
 * No information available at present.
 * 
 * FTRUNCATE (0x4604):
 * The parameter arg is a pointer to a LONG value that contains the new
 * length of the file to be truncated. The command is supported in MagiC as
 * of Version 3.0.
 * 
 * FIOEXECPT (0x4605):
 * In the parameter arg a pointer to a LONG value is returned which specifies
 * whether the file is in an exception state (1) or not (0).
 * 
 * The following commands are to be applied to all terminal devices such as
 * the console or a pseudo-terminal:
 * 
 * TIOCGETP (0x5400):
 * This command returns via the parameter arg a pointer to a sgttyb
 * structure, in which the parameters of the terminal are defined.
 * 
 * TIOCSETN (0x5401):
 * With this command one can set a new sgttyb structure which sets the
 * parameters of the terminal. You should first get the terminal control
 * parameters, modify what you wish to change, and then set them with this
 * call.
 * 
 * TIOCGETC (0x5402):
 * This command returns via the parameter arg a pointer to a tchars structure
 * in which the terminal control characters are defined.
 * 
 * TIOCSETC (0x5403):
 * With this command new terminal control characters can be set. For this one
 * passes a pointer in arg to a tchars structure in which the control
 * characters are defined. If a character is set to 0 then the corresponding
 * function will be disabled.
 * 
 * TIOCGLTC (0x5404):
 * This command returns via the parameter arg a pointer to a ltchars
 * structure in which extended terminal control characters are defined.
 * 
 * TIOCSLTC (0x5405):
 * With this command one can set new extended terminal control characters. To
 * do this, one passes in arg a pointer to a ltchars structure in which the
 * control characters are defined. If a character is set to 0 then the
 * corresponding function will be disabled.
 * 
 * TIOCGPGRP (0x5406):
 * This command returns via the parameter arg a pointer to the process group
 * ID of the terminal.
 * 
 * TIOCSPGRP (0x5407):
 * With this command one can set the process group ID of the terminal. To do
 * this, one passes in the parameter arg a pointer to the corresponding
 * process group. If processes from other groups should attempt to access
 * this terminal (read or write) then they will be sent job control signals
 * (SIGTSTP etc.).
 * 
 * TIOCFLUSH (0x5408):
 * With this command one can specify the type of flushing. For this, one
 * passes a pointer in the parameter arg to a LONG value that describes the
 * desired type. The following apply: Value Meaning
 */
int32_t Fcntl ( int16_t fh, int32_t arg, int16_t cmd )
{
	NOT_IMPLEMENTED(GDOS, Fcntl, 260);
	return -TOS_ENOSYS;
}

/**
 * Fcreate - 60
 *
 * The GEMDOS routine Fcreate creates a new file, or truncates an existing
 * one, with a given name and attributes. The following apply: Parameter
 * Meaning
 *
 * int16_t Fcreate ( const int8_t *fname, int16_t attr )
 */
int16_t Fcreate ( emuptr32_t fname, int16_t attr )
{
	NOT_IMPLEMENTED(GDOS, Fcreate, 60);
	return -TOS_ENOSYS;
}

/**
 * Fdatime - 87
 *
 * The GEMDOS routine Fdatime reads or sets the date and time of creation of
 * a file. The following apply: Parameter Meaning
 *
 * void Fdatime ( DOSTIME *timeptr, int16_t handle, int16_t wflag )
 */
void Fdatime ( emuptr32_t timeptr, int16_t handle, int16_t wflag )
{
	NOT_IMPLEMENTED(GDOS, Fdatime, 87);
}

/**
 * Fdelete - 65
 *
 * The GEMDOS routine Fdelete deletes the file designated by fname.
 * 
 * Note: The function should not be applied to opened files. As of MagiC 4,
 * deletion also works for files with the 'Hidden' and 'System' attributes.
 * In MagiC, furthermore, no symbolic links will be dereferenced, i.e. the
 * link will be deleted and not the file or the folder to which the link
 * points.
 *
 * int16_t Fdelete ( const int8_t *fname )
 */
int16_t Fdelete ( emuptr32_t fname )
{
	NOT_IMPLEMENTED(GDOS, Fdelete, 65);
	return -TOS_ENOSYS;
}

/**
 * Fdup - 69
 *
 * The GEMDOS routine Fdup duplicates a standard file handle (0-5) and
 * assigns it a new handle (>6) for a standard channel. The following apply:
 * handle Meaning
 */
int16_t Fdup ( int16_t handle )
{
	NOT_IMPLEMENTED(GDOS, Fdup, 69);
	return -TOS_ENOSYS;
}

/**
 * Ffchmod - 258
 *
 * See 'Name' above.
 */
int32_t Ffchmod ( int16_t fd, int16_t mode)
{
	NOT_IMPLEMENTED(GDOS, Ffchmod, 258);
	return -TOS_ENOSYS;
}

/**
 * Ffchown - 257
 *
 * This function changes ownership of an open file to uid and gid
 * respectively.
 * 
 * A non-privileged process can only change the ownership of a file that is
 * owned by this user to the effective group id of the process or one of its
 * supplementary groups.
 */
int32_t Ffchown ( int16_t fd, int16_t uid, int16_t gid )
{
	NOT_IMPLEMENTED(GDOS, Ffchown, 257);
	return -TOS_ENOSYS;
}

/**
 * Fforce - 70
 *
 * The GEMDOS routine Fforce redirects a standard channel to a specific other
 * channel created by the application. The following apply: stdh Channel to
 * be redirected 0 = Keyboard (stdin, con:)
 */
int16_t Fforce ( int16_t stdh, int16_t nonstdh )
{
	NOT_IMPLEMENTED(GDOS, Fforce, 70);
	return -TOS_ENOSYS;
}

/**
 * Fgetchar - 263
 *
 * The function reads a character from the file with the handle fh. The
 * parameter mode is only of interest when the opened file is a
 * (pseudo-)terminal. In that case the following values apply: mode Meaning
 */
int32_t Fgetchar ( int16_t fh, int16_t mode )
{
	NOT_IMPLEMENTED(GDOS, Fgetchar, 263);
	return -TOS_ENOSYS;
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
	NOT_IMPLEMENTED(GDOS, Fgetdta, 47);
	return -TOS_ENOSYS;
}

/**
 * Finstat - 261
 *
 * The function Finstat returns the maximum number of bytes that may be read
 * from a file with the handle fh without having to wait.
 * 
 * If the number of bytes cannot be reported exactly, then the value 1 will
 * be returned.
 * 
 * Note: In MagiC, the kernel first tries to implement the call in Fcntl
 * (FIONREAD). If this subfunction of dev_ioctl does not exist (i.e. the file
 * driver returns EINVFN), then dev_stat will be called. In that case this
 * can only result in the statement "Character available" (return == 1) or
 * "No character available" (return == 0). FAT files and (1link [shared
 * memory][Shared memory]) return the actual file length minus the current
 * position. Pipes return the number of bytes present in the block.
 */
int32_t Finstat ( int16_t fh )
{
	NOT_IMPLEMENTED(GDOS, Finstat, 261);
	return -TOS_ENOSYS;
}

/**
 * Flink - 301
 *
 * The function Flink creates a new name (newname) for the file named
 * oldname. The file can later be addressed under both names, and the
 * deletion (with Fdelete) of one name has no effect on the other.
 * 
 * One should note that oldname and newname have to lie on the same physical
 * drive, and also that not all file-systems offer the option of setting
 * links.
 *
 * int32_t Flink ( int8_t *oldname, int8_t *newname )
 */
int32_t Flink ( emuptr32_t oldname, emuptr32_t newname )
{
	NOT_IMPLEMENTED(GDOS, Flink, 301);
	return -TOS_ENOSYS;
}

/**
 * Flock - 92
 *
 * The GEMDOS routine Flock serves to protect portions of a file from access
 * by other processes. The following apply: Parameter Meaning
 */
int32_t Flock ( int16_t handle, int16_t mode, int32_t start, int32_t length )
{
	NOT_IMPLEMENTED(GDOS, Flock, 92);
	return -TOS_ENOSYS;
}

/**
 * Fmidipipe - 294
 *
 * The function Fmidipipe alters the GEMDOS handles for MIDI inout and output
 * for the process with the ID pid. The parameters in and out describe the
 * GEMDOS handles that take on MIDI input and output respectively for the
 * process.
 * 
 * In the case pid = 0, the current process will be used. Then The function
 * call is equivalent to the following two lines:Fforce (-4, in); Fforce (-5,
 * out); 
 */
int32_t Fmidipipe ( int16_t pid, int16_t in, int16_t out )
{
	NOT_IMPLEMENTED(GDOS, Fmidipipe, 294);
	return -TOS_ENOSYS;
}

/**
 * Fopen - 61
 *
 * The GEMDOS routine Fopen serves for opening specified files. The following
 * apply: Parameter Meaning
 *
 * int32_t Fopen ( const int8_t *fname, int16_t mode )
 */
int32_t Fopen ( emuptr32_t fname, int16_t mode )
{
	NOT_IMPLEMENTED(GDOS, Fopen, 61);
	return -TOS_ENOSYS;
}

/**
 * Foutstat - 262
 *
 * The function Foutstat returns the number of bytes that may be written to
 * the file with the handle fh without blocking the corresponding process.
 * 
 * If the exact number of bytes cannot be specified, then the value 1 will be
 * returned.
 * 
 * Note: In MagiC, the kernel first tries to implement the call in Fcntl
 * (FIONWRITE). If this subfunction of dev_ioctl does not exist (i.e. the
 * file driver returns EINVFN), then dev_stat will be called. In that case
 * this can only result in the statement "One character can be written"
 * (return == 1) or "No character can be written" (return == 0). FAT files
 * always return the value 1, shared memory returns the actual file length
 * minus the current position. Pipes return the number of bytes still free in
 * the block.
 */
int32_t Foutstat ( int16_t fh )
{
	NOT_IMPLEMENTED(GDOS, Foutstat, 262);
	return -TOS_ENOSYS;
}

/**
 * Fpipe - 256
 *
 * The function Fpipe creates a pipe that may be used for inter-process
 * communication (IPC). It is normally used by shells that wish to redirect
 * the input and output of their child processes; prior to launching the
 * child process, the shell redirects its input and output (as necessary) to
 * the read and write ends of the newly created file. On a successful call,
 * two GEMDOS handles will be returned in usrh.
 * 
 * usrh[0] denotes the handle for the read-end of the pipe, (read-only) while
 * usrh[1] the handle for the write-end of the pipe (write-only).
 * 
 * The pipe produced in this way will be created in the directory U:\PIPE as
 * a pseudo-file and will be named 'sys$pipe.xxx', where 'xxx' represent a
 * three-digit integer.
 *
 * int16_t Fpipe ( int16_t usrh[2] )
 */
int16_t Fpipe ( emuptr32_t usrh )
{
	NOT_IMPLEMENTED(GDOS, Fpipe, 256);
	return -TOS_ENOSYS;
}

/**
 * Fputchar - 264
 *
 * The function Fputchar writes a character to the file with the handle fh.
 * The parameter mode is only significant if the file is a (pseudo-)terminal:
 * mode Meaning
 */
int32_t Fputchar ( int16_t fh, int32_t ch, int16_t mode )
{
	NOT_IMPLEMENTED(GDOS, Fputchar, 264);
	return -TOS_ENOSYS;
}

/**
 * Fread - 63
 *
 * The GEMDOS routine Fread reads a given number of bytes from a file. The
 * following apply: Parameter Meaning
 *
 * int32_t Fread ( int16_t handle, int32_t count, void *buf )
 */
int32_t Fread ( int16_t handle, int32_t count, emuptr32_t buf )
{
	NOT_IMPLEMENTED(GDOS, Fread, 63);
	return -TOS_ENOSYS;
}

/**
 * Freadlink - 303
 *
 * The function Freadlink determines the filename of the symbolic link name.
 * This is the name that was specified for the first parameter of Fsymlink.
 * 
 * The name is stored in buffer buf, which has a length of bufsiz characters.
 *
 * int32_t Freadlink ( int16_t bufsiz, int8_t *buf, int8_t *name )
 */
int32_t Freadlink ( int16_t bufsiz, emuptr32_t buf, emuptr32_t name )
{
	NOT_IMPLEMENTED(GDOS, Freadlink, 303);
	return -TOS_ENOSYS;
}

/**
 * Frename - 86
 *
 * The GEMDOS routine Frename serves for renaming files. The new name may
 * also contain a complete access path within the relevant physical drive; in
 * that case the file will be moved. The following apply: Parameter Meaning
 *
 * int32_t Frename ( const int8_t *oldname, const int8_t *newname )
 */
int32_t Frename ( emuptr32_t oldname, emuptr32_t newname )
{
	NOT_IMPLEMENTED(GDOS, Frename, 86);
	return -TOS_ENOSYS;
}

/**
 * Fseek - 66
 *
 * The GEMDOS routine Fseek permits the file pointer to be moved to a new
 * position within a file. The following apply: Parameter Meaning
 */
int32_t Fseek ( int32_t offset, int16_t handle, int16_t seekmode )
{
	NOT_IMPLEMENTED(GDOS, Fseek, 66);
	return -TOS_ENOSYS;
}

/**
 * Fselect - 285
 *
 * The function Fselect checks which open files are ready for reading or
 * writing. The parameter rfds points to a LONG bitmap which describes the
 * set of GEMDOS read file descriptors to wait for.
 * 
 * In this longword, bit n is set when the file descriptor n is to be checked
 * for input. An empty set may also be represented optionally by a
 * NULL-pointer.
 * 
 * In the same way, wfds points to a set of write file descriptors whose
 * output status is to be checked.
 * 
 * On return of the function, the handles of the files that are ready for
 * reading or writing respectively and also should be checked are passed in
 * rfds and wfds.
 * 
 * Note that currently the only BIOS device Fselect works with is the
 * keyboard.
 * 
 * With timeout one can specify how many milliseconds one should wait before
 * the function returns. A value of 0 means in this case that the function
 * waits until one of the specified files is ready for reading or writing.
 * 
 * Example: Fselect (0, 0L, 0L, 0L) waits forever
 *
 * int32_t Fselect ( uint16_t timeout, int32_t *rfds, int32_t *wfds, ((int32_t) 0) )
 */
int32_t Fselect ( uint16_t timeout, emuptr32_t rfds, emuptr32_t wfds, ((int32_t) 0) )
{
	NOT_IMPLEMENTED(GDOS, Fselect, 285);
	return -TOS_ENOSYS;
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
	NOT_IMPLEMENTED(GDOS, Fsetdta, 26);
}

/**
 * Fsfirst - 78
 *
 * The GEMDOS routine Fsfirst can obtain information about the first
 * occurrence of a file or subdirectory. The following apply: Parameter
 * Meaning
 *
 * int32_t Fsfirst ( const int8_t *filename, int16_t attr )
 */
int32_t Fsfirst ( emuptr32_t filename, int16_t attr )
{
	NOT_IMPLEMENTED(GDOS, Fsfirst, 78);
	return -TOS_ENOSYS;
}

/**
 * Fsnext - 79
 *
 * The GEMDOS routine Fsnext searches for the next file entry that matches
 * the criteria specified in Fsfirst. It should be called as often as
 * necessary after a Fsfirst call to find all files that match the search
 * criteria.
 * 
 * Be aware that this function uses the application's DTA which initially
 * shares its memory location with the processes' command line. Hence using
 * this function without first assigning a new DTA will corrupt the command
 * line.
 * 
 * Note: If the program runs in the MiNT-domain, Fsnext will return lower
 * case filenames. As file-systems exist that are case sensitive, you should
 * avoid making such conversions manually. However, if you absolutely need
 * upper case filenames you should use Pdomain to make your program run in
 * the TOS-domain.
 */
int16_t Fsnext ( void )
{
	NOT_IMPLEMENTED(GDOS, Fsnext, 79);
	return -TOS_ENOSYS;
}

/**
 * Fsymlink - 302
 *
 * The function Fsymlink creates a symbolic link newname for the file named
 * oldname. If the call succeeds then the file can later be accessed under
 * both names. Deletion with Fdelete for newname does not affect the file
 * itself, but only the symbolic link. However, if Fdelete is applied to
 * oldname, then the file will be deleted as usual, and accesses to newname
 * are no longer possible.
 * 
 * The difference to hard links lies in the fact that symbolic links can
 * exist between different physical devices (and with that even different
 * file-system types).
 *
 * int32_t Fsymlink ( int8_t *oldname, int8_t *newname )
 */
int32_t Fsymlink ( emuptr32_t oldname, emuptr32_t newname )
{
	NOT_IMPLEMENTED(GDOS, Fsymlink, 302);
	return -TOS_ENOSYS;
}

/**
 * Fwrite - 64
 *
 * The GEMDOS routine Fwrite writes a given number of bytes to a specified
 * GEMDOS file. The following apply: Parameter Meaning
 *
 * int32_t Fwrite ( int16_t handle, int32_t count, void *buf )
 */
int32_t Fwrite ( int16_t handle, int32_t count, emuptr32_t buf )
{
	NOT_IMPLEMENTED(GDOS, Fwrite, 64);
	return -TOS_ENOSYS;
}

/**
 * Fxattr - 300
 *
 * The function Fxattr obtains the extended attributes of the file whose
 * complete name is specified in name, and saves these in xattr.
 * 
 * The parameter flag specifies how symbolic links are to be treated. The
 * following apply: flag Meaning
 *
 * int32_t Fxattr ( int16_t flag, int8_t *name, XATTR *xattr )
 */
int32_t Fxattr ( int16_t flag, emuptr32_t name, emuptr32_t xattr )
{
	NOT_IMPLEMENTED(GDOS, Fxattr, 300);
	return -TOS_ENOSYS;
}

