#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <dirent.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <utime.h>
#include <poll.h>

#include "common.h"
#include "path.h"
#include "tos_errors.h"
#include "gemdos.h"


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
	char* path = read_path(filename);
	if(!path)
		return TOS_EPTHNF;
	struct stat st;
	if(stat(path, &st) < 0)
	{
		attrib = MapErrno();
	}
	else
	{
		if(wflag)
		{
			if (attrib & 1)
			{
				st.st_mode &= ~0200;
			}
			else
			{
				st.st_mode |= 0200;
			}
			if (chmod(path, st.st_mode) != 0)
			{
				attrib = MapErrno();
				goto cleanup;
			}
		}
		attrib = 0;
		if((st.st_mode & S_IFMT) == S_IFDIR)
			attrib |= 0x10;
		if((st.st_mode & 0200) == 0)
			attrib |= 0x1;
	}
cleanup:
	free(path);
	return attrib;
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
int32_t Fchmod ( emuptr32_t filename, int16_t mode )
{
	int32_t retval = TOS_E_OK;
	char* path = read_path(filename);
	if(!path)
		return TOS_EPTHNF;
	if(chmod(path, mode) != 0)
		retval = MapErrno();
	free(path);
	return retval;
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
int32_t Fchown ( emuptr32_t filename, int16_t uid, int16_t gid )
{
	int32_t retval = TOS_E_OK;
	char* path = read_path(filename);
	if(!path)
		return TOS_EPTHNF;
	if(chown(path, uid, gid) != 0)
		retval = MapErrno();
	free(path);
	return retval;
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
	return close(handle);
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

/* some flags for TIOC[GS]FLAGS */
#define TF_BRKINT	0x0080		/* allow breaks interrupt (like ^C) */
#define TF_CAR		0x0800		/* nonlocal mode, require carrier */
#define TF_NLOCAL	TF_CAR

#define TF_STOPBITS	0x0003
#define TF_1STOP	0x001
#define TF_15STOP	0x002
#define TF_2STOP	0x003

#define TF_CHARBITS	0x000C
#define TF_8BIT	 	0x000
#define TF_7BIT	 	0x004
#define TF_6BIT	 	0x008
#define TF_5BIT	 	0x00C

int32_t Fcntl ( int16_t fh, int32_t arg, int16_t cmd )
{
	int32_t retval;
	if (cmd <=7)
		retval = fcntl(fh, cmd, arg, 0);
	else {
		pid_t nativePid;
		switch (cmd)
		{
			case 0x4600: // FSTAT
			retval = _Ffxattr(fh, arg);
			break;
			case 0x5400: // TIOCGETP
			{
				struct termios termios;
				if (tcgetattr(fh, &termios) == -1)
					return MapErrno();
				struct mint_sgttyb sg;
				sg.sg_ispeed=cfgetispeed(&termios);
				sg.sg_ospeed=cfgetospeed(&termios);
				sg.sg_flags=0;
				if((termios.c_iflag & (BRKINT|IGNBRK)) == BRKINT)
				{
					sg.sg_flags |= TF_BRKINT;
				}
				switch(termios.c_cflag & CSIZE)
				{
					case CS5:
					sg.sg_flags |= TF_5BIT;
					break;
					case CS6:
					sg.sg_flags |= TF_6BIT;
					break;
					case CS7:
					sg.sg_flags |= TF_7BIT;
					break;
					case CS8:
					sg.sg_flags |= TF_8BIT;
					break;
				}
				if(termios.c_cflag & CSTOPB)
				{
					sg.sg_flags |= TF_2STOP;
				}
				else
				{
					sg.sg_flags |= TF_1STOP;
				}
				retval = TOS_E_OK;
			}
			break;
			case 0x5406: //TIOCGPGRP
			retval = ioctl(fh, TIOCGPGRP, &nativePid);
			if( retval == -1)
			   return MapErrno();
			m68k_write_memory_32(arg, nativePid);
			//printf(" pid=%d ", m68k_read_memory_32(arg));
			break;
			case 0x5407: //TIOCSPGRP
			nativePid = m68k_read_memory_32(arg);
			retval = ioctl(fh, TIOCSPGRP, &nativePid);
			if( retval == -1)
			   return MapErrno();
			break;
			case 0x540b: //TIOCGWINSZ
			{
				struct winsize w;
				retval = ioctl(fh, TIOCGWINSZ, &w);
				if( retval == -1)
				   return MapErrno();
				printf("%d,%d,%d,%d\n",w.ws_row,w.ws_col, w.ws_xpixel, w.ws_ypixel);
				m68k_write_memory_16(arg, w.ws_row);
				m68k_write_memory_16(arg+2, w.ws_col);
				m68k_write_memory_16(arg+4, w.ws_xpixel);
				m68k_write_memory_16(arg+6, w.ws_ypixel);
				break;
			}
			case 0x540c: //TIOCSWINSZ
			{
				struct winsize w;
				w.ws_row = m68k_read_memory_16(arg);
				w.ws_col = m68k_read_memory_16(arg+2);
				w.ws_xpixel = m68k_read_memory_16(arg+4);
				w.ws_ypixel = m68k_read_memory_16(arg+6);
				retval = ioctl(fh, TIOCSWINSZ, &w);
				if( retval == -1)
				   return MapErrno();
				break;
			}
			default:
			retval = TOS_ENOSYS;
			NOT_IMPLEMENTED(GEMDOS, Fcntl_cmd, cmd);
		}
	}
	return retval;
}

// Mint file types are different from linux
#define MINT_S_IFSOCK      0010000 /* Socket.  */
#define MINT_S_IFCHR       0020000 /* Character device.  */
#define MINT_S_IFDIR       0040000 /* Directory.  */
#define MINT_S_IFBLK       0060000 /* Block device.  */
#define MINT_S_IFREG       0100000 /* Regular file.  */
#define MINT_S_IFIFO       0120000 /* FIFO.  */
#define MINT_S_IFMEM       0140000 /* memory region or process */
#define MINT_S_IFLNK       0160000 /* Symbolic link.  */

static uint32_t convert_mode(mode_t st_mode)
{
	mode_t mint_mode = st_mode & 007777; // permission bits are the same
	switch(st_mode & S_IFMT)
	{
		case S_IFSOCK:
		{
			mint_mode |= MINT_S_IFSOCK;
			break;
		}
		case S_IFLNK:
		{
			mint_mode |= MINT_S_IFLNK;
			break;
		}
		case S_IFREG:
		{
			mint_mode |= MINT_S_IFREG;
			break;
		}
		case S_IFBLK:
		{
			mint_mode |= MINT_S_IFBLK;
			break;
		}
		case S_IFDIR:
		{
			mint_mode |= MINT_S_IFDIR;
			break;
		}
		case S_IFCHR:
		{
			mint_mode |= MINT_S_IFCHR;
			break;
		}
		case S_IFIFO:
		{
			mint_mode |= MINT_S_IFIFO;
			break;
		}
		default:
		{
			fprintf(stderr, "Warning: Unknown S_IFMT %0x\n", (st_mode & S_IFMT));
			mint_mode |= MINT_S_IFREG;
		}
	}
	return mint_mode;
}

static void convert_stat(const struct stat* st, uint32_t address)
{
#	define WRITE(field, value) m68k_write_field(address, struct mint_stat, field, value)
	WRITE(mst_dev, st->st_dev);
	WRITE(mst_ino, st->st_ino);
	WRITE(mst_mode, convert_mode(st->st_mode));
	WRITE(mst_nlink, st->st_nlink);
	WRITE(mst_uid, st->st_uid);
	WRITE(mst_gid, st->st_gid);
	WRITE(mst_rdev, st->st_rdev);
	WRITE(mst_high_atime, 0);
	WRITE(mst_atime, st->st_atime);
	WRITE(mst_atim, st->st_atime);
	WRITE(mst_high_mtime, 0);
	WRITE(mst_mtime, st->st_mtime);
	WRITE(mst_mtim, st->st_mtime);
	WRITE(mst_high_ctime, 0);
	WRITE(mst_ctime, st->st_ctime);
	WRITE(mst_ctim, st->st_ctime);
	WRITE(mst_size, st->st_size);
	WRITE(mst_blocks, st->st_blocks);
	WRITE(mst_blksize, st->st_blksize);
	WRITE(mst_flags, 0);
	WRITE(mst_gen, 0);
#	undef WRITE
}

static void convert_xattr(const struct stat* st, uint32_t address)
{
#	define WRITE(field, value) m68k_write_field(address, XATTR, field, value)
	WRITE(mode, convert_mode(st->st_mode) & 0xffff);
	WRITE(index, st->st_ino);
	WRITE(dev, st->st_dev);
	WRITE(rdev, st->st_rdev);
	WRITE(nlink, st->st_nlink);
	WRITE(uid, st->st_uid);
	WRITE(gid, st->st_gid);
	WRITE(size, st->st_size);
	WRITE(blksize, st->st_blksize);
	WRITE(nblocks, st->st_blocks);
	WRITE(mdostime, unixtime2dos(&st->st_mtime));
	WRITE(adostime, unixtime2dos(&st->st_atime));
	WRITE(cdostime, unixtime2dos(&st->st_ctime));
	uint32_t attrib = 0;
	if((st->st_mode & S_IFMT) == S_IFDIR)
		attrib |= 0x10;
	if((st->st_mode & 0200) == 0)
		attrib |= 0x1;

	WRITE(attr, attrib);
	WRITE(reserved2, 0);
	WRITE(reserved3, 0);
#	undef WRITE
}

int16_t Ffstat64 (int16_t fd, /*struct stat */ uint32_t address)
{

	//printf("Ffstat64(%d, 0x%08x)\n", fd, address);
	struct stat st;
	if(fstat(fd, &st) < 0)
	{
		return MapErrno();
	}
	else
	{
		convert_stat(&st, address);
	}
	return 0;
}

int16_t Fstat64 (uint16_t lflag, uint32_t name, /*struct stat */ uint32_t address)
{
	int16_t retval = TOS_E_OK;
	char* path = read_path(name);
	if(!path)
		return TOS_EPTHNF;
	//printf("Fstat64(%d, %s, 0x%08x)\n", lflag, buffer, address);
	struct stat st;
	if((lflag?lstat(path,&st):stat(path, &st)) < 0)
	{
		retval = MapErrno();
	}
	else
	{
		convert_stat(&st, address);
	}
	free(path);
	return retval;
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
	return Fopen(fname, 0x602);
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
	if(!wflag)
	{
		struct stat st;
		if(fstat(handle, &st) == 0)
		{
			m68k_write_memory_32(timeptr, unixtime2dos(&st.st_mtime));
		}
	}
	else
	{
		struct timeval tv[2];
		tv[0].tv_sec = tv[1].tv_sec = dostime2unix(m68k_read_memory_32(timeptr));
		tv[0].tv_usec = tv[1].tv_usec = 0;
		futimes(handle, tv);
	}
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
int16_t Fdelete ( emuptr32_t filename )
{
	int16_t retval = TOS_E_OK;
	char* path = read_path(filename);
	if(!path)
		return TOS_EPTHNF;
	if (unlink(path) < 0)
		retval = MapErrno();
	free(path);
	return retval;
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
	return dup(handle);
}

/**
 * Ffchmod - 258
 *
 * See 'Fchmod' above.
 */
int32_t Ffchmod ( int16_t fd, int16_t mode)
{
	if(fchmod(fd, mode) != 0)
		return MapErrno();
	return TOS_E_OK;
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
	if(fchown(fd, uid, gid) != 0)
		return MapErrno();
	return TOS_E_OK;
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
	//NOT_IMPLEMENTED(GEMDOS, Fforce, stdh);
	//NOT_IMPLEMENTED(GEMDOS, Fforce, nonstdh);
	if ( dup2(nonstdh, stdh) == -1)
		return MapErrno();
	return TOS_E_OK;
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
	char c;
	if(read(fh, &c, 1) > 0)
		return MapErrno();
	else
		return c;
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
	NOT_IMPLEMENTED(GEMDOS, Finstat, 261);
	return TOS_ENOSYS;
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
	int32_t retval = TOS_E_OK;
	char* oldpath = read_path(oldname);
	if(!oldpath)
		return TOS_EPTHNF;
	char* newpath = read_path(newname);
	if(!newpath)
	{
		free(oldpath);
		return TOS_EPTHNF;
	}
	if (link(oldpath, newpath) < 0)
		retval = MapErrno();
	free(oldpath);
	free(newpath);
	return retval;
}

/**
 * Flock - 92
 *
 * The GEMDOS routine Flock serves to protect portions of a file from access
 * by other processes. The following apply: Parameter Meaning
 */
int32_t Flock ( int16_t handle, int16_t mode, int32_t start, int32_t length )
{
	NOT_IMPLEMENTED(GEMDOS, Flock, 92);
	return TOS_ENOSYS;
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
	NOT_IMPLEMENTED(GEMDOS, Fmidipipe, 294);
	return TOS_ENOSYS;
}

/**
 * Fopen - 61
 *
 * The GEMDOS routine Fopen serves for opening specified files. The following
 * apply: Parameter Meaning
 *
 * int32_t Fopen ( const int8_t *fname, int16_t mode )
 */
int32_t Fopen ( emuptr32_t filename, int16_t mode )
{
	int16_t retval = TOS_E_OK;
	char* path = read_path(filename);
	if(!path)
		return TOS_EPTHNF;

	int flags = 0;
 	switch(mode & 3)
	{
		case 0:
		flags = O_RDONLY;
		break;
		case 1:
		flags = O_WRONLY;
		break;
		case 2:
		flags = O_RDWR;
		break;
	}

	if (mode & 0x08)
	{
		flags |= O_APPEND;
	}
	if (mode & 0x200)
	{
		flags |= O_CREAT;
	}
	if (mode & 0x400)
	{
		flags |= O_TRUNC;
	}
	if (mode & 0x800)
	{
		flags |= O_EXCL;
	}
	if (mode & 0x4000)
	{
		flags |= O_NOCTTY;
	}
	if (mode & 0x04)
	{
		flags |= O_NOATIME;
	}
	if (mode & 0x10000)
	{
		flags |= O_DIRECTORY;
	}
	if (mode & 0x20000)
	{
		flags |= O_NOFOLLOW;
	}
	if (mode & 0x100)
	{
		flags |= O_NONBLOCK;
	}

	retval = open(path, flags, 0777);
	if (retval < 1)
		retval = MapErrno();

	free(path);
	return retval;
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
	NOT_IMPLEMENTED(GEMDOS, Foutstat, 262);
	return TOS_ENOSYS;
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
	int pipefd[2];
	int retval = pipe(pipefd);
	if(retval != 0)
	{
		return MapErrno();
	}
	m68k_write_memory_16(usrh, pipefd[0]);
	m68k_write_memory_16(usrh+2, pipefd[1]);
	return 0;
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
	char c = (char)ch;
	int retval = write(fh, &c, 1);
	if (retval < 0)
	{
		return MapErrno();
	}
	return retval;

}

/**
 * Fread - 63
 *
 * The GEMDOS routine Fread reads a given number of bytes from a file. The
 * following apply: Parameter Meaning
 *
 * int32_t Fread ( int16_t handle, int32_t count, void *buf )
 */
int32_t Fread ( int16_t handle, int32_t count, emuptr32_t address )
{
	uint8_t* buffer = &memory[address];
	int32_t bytes_read = read(handle, buffer, count);
	if (bytes_read < 0)
	{
		return MapErrno();
	}
	return bytes_read;
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
int32_t Freadlink ( int16_t bufsiz, emuptr32_t buf, emuptr32_t filename )
{
	int32_t retval = TOS_E_OK;
	char* path = read_path(filename);
	if(!path)
		return TOS_EPTHNF;
	char* buffer = &memory[buf];
	retval = readlink(path, buffer, bufsiz);
	if(retval < 0)
		retval = MapErrno();
	free(path);
	return retval;
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
	int32_t retval = TOS_E_OK;
	char* oldpath = read_path(oldname);
	if(!oldpath)
		return TOS_EPTHNF;
	char* newpath = read_path(newname);
	if(!newpath)
	{
		free(oldpath);
		return TOS_EPTHNF;
	}
	if (rename(oldpath, newpath) < 0)
		retval = MapErrno();
	free(oldpath);
	free(newpath);
	return retval;
}

/**
 * Fseek - 66
 *
 * The GEMDOS routine Fseek permits the file pointer to be moved to a new
 * position within a file. The following apply: Parameter Meaning
 */
int32_t Fseek ( int32_t offset, int16_t handle, int16_t seekmode )
{
	return lseek(handle, offset, seekmode);
}

static inline int popcount(uint32_t n)
{
#if (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))) \
	|| (defined(__clang__) && (__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 1)))
	return __builtin_popcount(n);
#else
     n -= (n >> 1) & 0x55555555;
     n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
     return (((n + (n >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
#endif
}

// From http://graphics.stanford.edu/~seander/bithacks.html
static inline int lastbit(uint32_t v)
{
	uint32_t c;     // c will be the number of zero bits on the right,
	                // so if v is 1101000 (base 2), then c will be 3
					// NOTE: if 0 == v, then c = 31.
	if (v & 0x1)
	{
		// special case for odd v (assumed to happen half of the time)
		return 0;
	}
	c = 1;
	if ((v & 0xffff) == 0)
	{
		v >>= 16;
		c += 16;
	}
	if ((v & 0xff) == 0)
	{
		v >>= 8;
		c += 8;
	}
	if ((v & 0xf) == 0)
	{
		v >>= 4;
		c += 4;
	}
	if ((v & 0x3) == 0)
	{
		v >>= 2;
		c += 2;
	}
	c -= v & 0x1;
	return c;
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
int32_t Fselect ( uint16_t timeout, emuptr32_t rfdsp, emuptr32_t wfdsp, emuptr32_t efdsp )
{
	int32_t mask = 0;
	int32_t rfds = 0;
	int32_t wfds = 0;
	int32_t efds = 0;
	if(rfdsp)
	{
		rfds = m68k_read_memory_32(rfdsp);
		mask |= rfds;
	}
	if(wfdsp)
	{
		wfds = m68k_read_memory_32(wfdsp);
		mask |= wfds;
	}
	if(efdsp)
	{
		efds = m68k_read_memory_32(efdsp);
		mask |= efds;
	}

	int32_t nfds = popcount(mask);
	struct pollfd *native_fds = alloca(nfds*sizeof(struct pollfd));
	for(int i=0; i<nfds; i++)
	{
		native_fds[i].fd = lastbit(mask);
		native_fds[i].events = 0;
		native_fds[i].revents = 0;
		uint32_t fdmask = (1 << native_fds[i].fd);

		if(rfdsp && rfds & fdmask)
		{
			native_fds[i].events |= POLLIN;
		}
		if(wfdsp && wfds & fdmask)
		{
			native_fds[i].events |= POLLOUT;
		}
		if(efdsp && efds & fdmask)
		{
			native_fds[i].events |= POLLPRI;
		}

		mask &= mask-1; // Clear last significant bit in mask
	}

	int32_t status = poll(native_fds, nfds, timeout==0?-1:timeout);
	if(status == -1)
	{
		return MapErrno();
	}

	rfds = wfds = efds = 0;
	for(int i=0; i<nfds; i++)
	{
		uint32_t fdmask = (1 << native_fds[i].fd);
		if(rfdsp && native_fds[i].revents & POLLIN)
		{
			rfds |= fdmask;
		}
		if(wfdsp && native_fds[i].revents & POLLOUT)
		{
			wfds |= fdmask;
		}
		if(efdsp && native_fds[i].revents & (POLLERR | POLLPRI | POLLHUP))
		{
			efds |= fdmask;
		}
	}
	if(rfdsp)
		m68k_write_memory_32(rfdsp, rfds);
	if(wfdsp)
		m68k_write_memory_32(wfdsp, wfds);
	if(efdsp)
		m68k_write_memory_32(efdsp, efds);

	return popcount(rfds | wfds | efds);
}

#define MINT_POLLIN		0x001	/* There is data to read */
#define MINT_POLLPRI	0x002	/* There is urgent data to read */
#define MINT_POLLOUT	0x004	/* Writing now will not block */
#define MINT_POLLRDNORM	0x040	/* Normal data may be read */
#define MINT_POLLRDBAND	0x080	/* Priority data may be read */
#define MINT_POLLWRNORM	0x100	/* Writing now will not block */
#define MINT_POLLWRBAND	0x200	/* Priority data may be written */
#define MINT_POLLMSG	0x400
#define MINT_POLLERR	0x0008
#define MINT_POLLHUP	0x0010
#define MINT_POLLNVAL	0x0020

#if MINT_POLLIN == POLLIN && \
	MINT_POLLPRI == POLLPRI && \
	MINT_POLLOUT == POLLOUT && \
	MINT_POLLRDNORM == POLLRDNORM && \
	MINT_POLLRDBAND == POLLRDBAND && \
	MINT_POLLWRNORM == POLLWRNORM && \
	MINT_POLLWRBAND == POLLWRBAND && \
	MINT_POLLMSG == POLLMSG && \
	MINT_POLLERR == POLLERR && \
	MINT_POLLHUP == POLLHUP && \
	MINT_POLLNVAL == POLLNVAL
// Mint and plaform poll flags are identical
#define mint_poll2native(x) (x)
#define native_poll2mint(x) (x)
#else
#error "todo - Map mint poll flags to native platform"
#endif

/**
 * Fpoll - 346
 *
 * int32_t Fpoll ( POLLFD *fds, uint32_t nfds, uint32_t timeout )
 */
int32_t Fpoll ( /*POLLFD */ emuptr32_t fds, uint32_t nfds, uint32_t timeout )
{
#	define READ(index, field) m68k_read_field(fds + (index)*sizeof(struct mint_pollfd), struct mint_pollfd, field)
#	define WRITE(index, field, value) m68k_write_field(fds + (index)*sizeof(struct mint_pollfd), struct mint_pollfd, field, (value))
	int32_t retval;
	struct pollfd *native_fds = alloca(nfds*sizeof(struct pollfd));
	for(int i=0; i<nfds; i++)
	{
		native_fds[i].fd = READ(i, fd);
		native_fds[i].events = mint_poll2native(READ(i, events));
		native_fds[i].revents = mint_poll2native(READ(i, revents)); // or 0?
	}
	retval = poll(native_fds, nfds, timeout);
	if (retval < 0)
	{
		retval = MapErrno();
	}
	else
	{
		for(int i=0; i<nfds; i++)
		{
			//WRITE(i, fd, native_fds[i].fd);
			//WRITE(i, events, native_poll2mint(native_fds[i].events));
			WRITE(i, revents, native_poll2mint(native_fds[i].revents));
		}
	}
	return retval;
#	undef READ
#	undef WRITE
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

/**
 * Fsfirst - 78
 *
 * The GEMDOS routine Fsfirst can obtain information about the first
 * occurrence of a file or subdirectory. The following apply: Parameter
 * Meaning
 *
 * int32_t Fsfirst ( const int8_t *filename, int16_t attr )
 */
struct DTAPrivate
{
	DIR* dirh;
	int16_t attr;
	bool glob;
	char pattern[1];
};

int32_t Fsfirst ( emuptr32_t filename, int16_t attr )
{
	char* path = read_path(filename);
	if(!path)
		return TOS_EPTHNF;
	int s=strlen(path);

	char* dir = path;
	char* pattern = path;
	bool glob = false;
	for(char* c = path+s-1;c>=path;c--)
	{
		//printf("%s\n", c);
		if(*c == '/')
		{
			*c = '\0';
			pattern=c+1;
			break;
		}
		if (*c == '*' || *c == '?')
		{
			glob = true;
		}
	}
	if (pattern == dir)
	{
		dir = ".";
	}
	if (strcmp("*.*", pattern) == 0 )
	{
		pattern = "";
	}

	struct DTAPrivate* priv = calloc(sizeof(struct DTAPrivate)+strlen(pattern),1);
	strcpy(priv->pattern, pattern);
	priv->attr = attr;
	priv->glob = glob;
	priv->dirh = opendir(dir);

	emuptr32_t current_dta = m68k_read_field(current_process, basepage_t, p_dta);

	free(path);
	if (!priv->dirh)
	{
		free(priv);
		m68k_write_memory_64(current_dta, (uint64_t)0);
		return TOS_EFILNF;
	}
	else
	{
		m68k_write_memory_64(current_dta, (uint64_t)priv);
		int16_t retval = Fsnext();
		if (retval == TOS_ENMFIL)
			return TOS_EFILNF;
		else
			return retval;
	}
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
	emuptr32_t current_dta = m68k_read_field(current_process, basepage_t, p_dta);
	int16_t retval = TOS_ENMFIL;
	bool glob = true;
	struct DTAPrivate* priv = (struct DTAPrivate*)m68k_read_memory_64(current_dta);
	if (!priv || !priv->dirh)
	{
		goto cleanup;
	}
	glob = priv->glob;
	struct dirent* entry;
	struct stat fileStat;
	int attrib;
	char fn83[13];
	while(true)
	{
		attrib = 0;
		entry = readdir(priv->dirh);
		if (!entry)
		{
			goto cleanup;
		}

		// .. and . only match exactly
		if (glob && (strcmp("..", entry->d_name) == 0 || strcmp(".", entry->d_name) == 0))
		{
			continue;
		}

		filename8_3(fn83, entry->d_name);
		if (priv->pattern[0] == 0
			|| strcasecmp(priv->pattern, entry->d_name) == 0
			|| strcasecmp(priv->pattern, fn83) == 0)
		{
			fstatat(dirfd(priv->dirh),entry->d_name, &fileStat, 0);
			if(entry->d_name[0] == '.')
				attrib |= 0x02;
			if((fileStat.st_mode & S_IFMT) == S_IFDIR)
				attrib |= 0x10;
			if((fileStat.st_mode & 0200) == 0)
				attrib |= 0x1;
			if ((attrib & priv->attr) == attrib)
				break;
		}
	}
	retval = TOS_E_OK;
	struct tm* t = localtime(&fileStat.st_mtime);
	uint16_t time = ((t->tm_sec/2)&0xF) | (t->tm_min << 5) | (t->tm_hour << 11);
	uint16_t date = t->tm_mday | ((t->tm_mon+1)<<5) | (((t->tm_year-80)&0x7f)<<9);
	m68k_write_field(current_dta, struct DTA, d_time, time);
	m68k_write_field(current_dta, struct DTA, d_date, date);
	m68k_write_field(current_dta, struct DTA, d_length, (int32_t)fileStat.st_size);
	m68k_write_field(current_dta, struct DTA, d_attrib, attrib);
	m68k_write_string(current_dta+offsetof(struct DTA, d_fname), fn83, 13);

cleanup:
	if (priv && (retval <0 || !glob))
	{
		if (priv->dirh)
			closedir(priv->dirh);
		free(priv);
		m68k_write_memory_64(current_dta, (uint64_t)0);
	}
	return retval;
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
	int32_t retval = TOS_E_OK;
	char* oldpath = read_path(oldname);
	if(!oldpath)
		return TOS_EPTHNF;
	char* newpath = read_path(newname);
	if(!newpath)
	{
		free(oldpath);
		return TOS_EPTHNF;
	}
	if (symlink(oldpath, newpath) < 0)
		retval = MapErrno();
	free(oldpath);
	free(newpath);
	return retval;
}

/**
 * Fwrite - 64
 *
 * The GEMDOS routine Fwrite writes a given number of bytes to a specified
 * GEMDOS file. The following apply: Parameter Meaning
 *
 * int32_t Fwrite ( int16_t handle, int32_t count, void *buf )
 */
int32_t Fwrite ( int16_t handle, int32_t count, emuptr32_t address )
{
	uint8_t* buffer = &memory[address];
	int32_t bytes_written = write(handle, buffer, count);
	if (bytes_written < 0)
	{
		return MapErrno();
	}
	return bytes_written;
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
int32_t Fxattr ( int16_t lflag, emuptr32_t filename, emuptr32_t xattr )
{
	int32_t retval = TOS_E_OK;
	char* path = read_path(filename);
	if(!path)
		return TOS_EPTHNF;
	struct stat st;
	if((lflag?lstat(path,&st):stat(path, &st)) < 0)
	{
		retval = MapErrno();
	}
	else
	{
		convert_xattr(&st, xattr);
	}
	free(path);
	return retval;
}

/* _Ffxattr is called via Fcntl, setting cmd to FSTAT(0x4600) */
int32_t _Ffxattr ( int16_t handle, emuptr32_t xattr )
{
	int32_t retval = TOS_E_OK;
	struct stat st;
	if(fstat(handle, &st) < 0)
	{
		retval = MapErrno();
	}
	else
	{
		convert_xattr(&st, xattr);
	}
	return retval;
}
