#include <unistd.h>
#include <alloca.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include "common.h"
#include "gemdos.h"
#include "tos_errors.h"

/**
 * Dchroot - 330
 *
 * The path parameter is a pathname of the folder that will be the new root
 * directory for the calling process. Directories and files which are outside
 * of the specified tree will no longer be accessible.
 *
 * Dchroot does not automatically change the current directory to the newly
 * specified root. Also, the call needs root privileges to operate properly.
 * This function is used by e.g. ftpd to limit the user privileges in
 * accessing file-systems.
 *
 * int32_t Dchroot( int8_t *path )
 */
int32_t Dchroot( emuptr32_t path )
{
	NOT_IMPLEMENTED(GEMDOS, Dchroot, 330);
	return TOS_ENOSYS;
}

/**
 * Dclosedir - 299
 *
 * The function Dclosedir closes the directory with the handle dirhandle.
 */
int32_t Dclosedir ( int32_t dirhandle )
{
	int32_t retval = TOS_E_OK;
	DIR* dirP = (DIR*)m68k_read_memory_64(dirhandle);
	retval = Mfree(dirhandle);
	if (retval < 0)
		return retval;
	if (!closedir(dirP))
		return TOS_EIHNDL;
	return retval;
}

/**
 * Dcntl - 304
 *
 * The function Dcntl executes the command cmd on the file or the directory
 * name. Details depend on the file-system to which name refers. The meaning
 * of the parameter arg is in turn dependent on cmd.
 *
 * Normally the Dcntl call is supported by the file-systems U:\\ and U:\DEV.
 *
 * The unified file-system U:\ knows the following commands: Command
 * Description
 *
 * int32_t Dcntl ( int16_t cmd, int8_t *name, int32_t arg )
 */
int32_t Dcntl ( int16_t cmd, emuptr32_t name, int32_t arg )
{
	NOT_IMPLEMENTED(GEMDOS, Dcntl, 304);
	return TOS_ENOSYS;
}

/**
 * Dcreate - 57
 *
 * The GEMDOS routine Dcreate creates a directory with the pathname path,
 * which has to be terminated with the ASCII character 0, on the specified
 * drive.
 *
 * Note: Due to defective error-handling in GEMDOS Versions MagiC 3, the call
 * is passed on by the kernel as xfs_dcreate with creation mode Fxattr
 * %0100000111101101 (i.e. a 'directory file' with access permissions
 * RWXRwXRwX). The XFS should not delete any files or subdirectories of the
 * same name, but return the error-code EACCDN in this case. Invalid
 * filenames "." or ".." must also be intercepted by the XFS.
 *
 * int32_t Dcreate ( CONST int8_t *path )
 */
int32_t Dcreate ( emuptr32_t path )
{
	NOT_IMPLEMENTED(GEMDOS, Dcreate, 57);
	return TOS_ENOSYS;
}

/**
 * Ddelete - 58
 *
 * The GEMDOS routine Ddelete deletes a directory with the pathname path,
 * which may not contain any files or subdirectories. The string path must be
 * terminated with the ASCII character 0.
 *
 * Note: In GEMDOS Versions Dcreate with an immediately following Ddelete did
 * not work - only a further Ddelete achieved the desired result.
 *
 * In MagiC the kernel first checks whether the directory is a current path
 * and if appropriate returns an error-message. If possible, only empty
 * directories should be deleted; this is however up to the XFS. As of MagiC
 * Version 4.01, symbolic links too can be deleted with this function; older
 * versions always deleted the directory to which the link pointed.
 *
 * int32_t Ddelete ( CONST int8_t *path )
 */
int32_t Ddelete ( emuptr32_t path )
{
	NOT_IMPLEMENTED(GEMDOS, Ddelete, 58);
	return TOS_ENOSYS;
}

/**
 * Dfree - 54
 *
 * The GEMDOS routine Dfree obtains the capacity and current occupancy of a
 * drive. The following apply: Parameter Meaning
 *
 * int16_t Dfree ( DISKINFO *buf, int16_t driveno )
 */
int16_t Dfree ( emuptr32_t buf, int16_t driveno )
{
	NOT_IMPLEMENTED(GEMDOS, Dfree, 54);
	return TOS_ENOSYS;
}

char* get_current_dir_name(void);

/**
 * Dgetcwd - 315
 *
 * The current working directory of the active process in drive drv is
 * returned. The parameter size gives the size of the buffer for
 * accommodating the name.
 *
 * int32_t Dgetcwd ( int8_t *path, int16_t drv, int16_t size )
 */
int32_t Dgetcwd ( emuptr32_t path, int16_t driveno, int16_t size )
{
	int32_t retval = TOS_EDRIVE;
	if(driveno == 0 || driveno == 'u'-'a'+1)
	{
		char* unix_path=get_current_dir_name();
		size_t required = strlen(unix_path)+1;
		if(size >= required)
		{
			m68k_write_string(path, unix_path, size);
			retval = TOS_E_OK;
		}
		else
		{
			retval = TOS_ENAMETOOLONG;
		}
		free(unix_path);
	}
	return retval;
}

/**
 * Dgetdrv - 25
 *
 * The GEMDOS routine Dgetdrv obtains the current drive.
 */
int16_t Dgetdrv ( void )
{
	return 'u' - 'a';
}

/**
 * Dgetpath - 71
 *
 * The GEMDOS routine Dgetpath obtains the current directory on the drive
 * driveno. The designation of the drives are coded in the parameter driveno
 * as follows: driveno Meaning
 *
 * int16_t Dgetpath ( int8_t *path, int16_t driveno )
 */
int16_t Dgetpath ( emuptr32_t path, int16_t driveno )
{
	if(driveno == 0 || driveno == 'u'-'a'+1)
	{
		char* unix_path=get_current_dir_name();
		char* tmp=alloca(strlen(unix_path)+1);

		char* d=tmp;
		for(const char* s=unix_path;*s;)
		{
			if(*s == '/')
			{
				*d++='\\';
				s++;
			}
			else
			{
				s=filename8_3(d,s);
				while(*d)
					d++;
			}
		}
		m68k_write_string(path, tmp, 1024);
		free(unix_path);
		return TOS_E_OK;
	}
	else
	{
		return TOS_EDRIVE;
	}
}

/**
 * Dlock - 309
 *
 * The function Dlock permits the locking of the BIOS device drv, or
 * releasing it again. On a locked drive no GEMDOS file operations are
 * permitted; Rwabs is only allowed for the locking process.
 *
 * The purpose of the function is to allow low-level programs (such as those
 * for formatting) to perform (X)BIOS functions, while the device remains
 * locked for normal file accesses.
 *
 * Bit 0 of mode = 1: Lock device
 */
int32_t Dlock ( int16_t mode, int16_t drv )
{
	NOT_IMPLEMENTED(GEMDOS, Dlock, 309);
	return TOS_ENOSYS;
}

/**
 * Dopendir - 296
 *
 * The function Dopendir opens the directory name for reading. name should be
 * NULL-terminated and should not contain a trailing backslash. The parameter
 * flag describes the manner in which the directory is to be opened: flag
 * Meaning
 *
 * int32_t Dopendir ( int8_t *name, int16_t flag )
 */
int32_t Dopendir ( emuptr32_t name, int16_t flag )
{
	if(flag == 1)
		return TOS_ENOSYS;
	char buffer[1024];
	m68k_read_string(name, buffer, 1023, 1);
	DIR* dirP = opendir(buffer);
	if(dirP)
	{
		int32_t dirH = Malloc(16); // Allocate memory to store the dir pointer
		m68k_write_memory_64(dirH, (uint64_t)dirP);
		return dirH;
	}
	else
	{
		return TOS_EPTHNF;
	}
}

/**
 * Dpathconf - 292
 *
 * The function Dpathconf makes it possible to obtain information about the
 * file-system that contains the file name.
 *
 * The parameter mode specifies which possibilities or restrictions of the
 * file-systems are to be inquired. Possible values here are: mode Meaning
 *
 * int32_t Dpathconf ( BYTE *name, int16_t mode )
 */
int32_t Dpathconf ( emuptr32_t name, int16_t mode )
{
	switch (mode)
	{
		case 0:
		return 1024;
		case 2:
		return PATH_MAX;
		case 3:
		return NAME_MAX;
		case 6:
		return 0;
	}
	NOT_IMPLEMENTED(GEMDOS, Dpathconf_mode, mode);
	return TOS_ENOSYS;
}

/**
 * Dreaddir - 297
 *
 * The function Dreaddir returns the next file in the directory dirhandle.
 * The filename and an optional 4-byte index for the file are written to buf.
 *
 * The file index is omitted if the directory was opened im compatibility
 * mode, otherwise it appears as the first in buf followed by a
 * (NULL-terminated) filename.
 *
 * (Possibly different) names with the same index belong to the same file.
 * (Possibly same) names with different indexes belong to different files.
 *
 * Note about MagiC: For FAT file-systems the function returns the following
 * index data: Directories: Start cluster in Motorola format Other files:
 * High word = Start cluster of the directory
 *
 * int32_t Dreaddir ( int16_t len, int32_t dirhandle, int8_t *buf )
 */
int32_t Dreaddir ( int16_t len, int32_t dirhandle, emuptr32_t buf )
{
	int32_t retval = TOS_E_OK;
	DIR* dirP = (DIR*)m68k_read_memory_64(dirhandle);
	struct dirent *entry = readdir(dirP);
	if(! entry)
	{
		return errno?TOS_EIHNDL:TOS_ENMFIL;
	}
	m68k_write_memory_32(buf, entry->d_ino&0xffffffff);
	m68k_write_string(buf+4, entry->d_name, NAME_MAX);
	return TOS_E_OK;

}

/**
 * Dreadlabel - 338
 *
 * The function Dreadlabel returns in label the name of the file-system lying
 * in path. The parameter length specifies the size of the receiving buffer
 * for label.
 *
 * int32_t Dreadlabel ( CONST int8_t *path, int8_t *label, int16_t length )
 */
int32_t Dreadlabel ( emuptr32_t path, emuptr32_t label, int16_t length )
{
	NOT_IMPLEMENTED(GEMDOS, Dreadlabel, 338);
	return TOS_ENOSYS;
}

/**
 * Drewinddir - 298
 *
 * The function Drewinddir resets the directory handle to the start, so that
 * the next call of Dreaddir reads the first entry in the directory once
 * more.
 */
int32_t Drewinddir ( int32_t dirhandle )
{
	int32_t retval = TOS_E_OK;
	DIR* dirP = (DIR*)m68k_read_memory_64(dirhandle);
	rewinddir(dirP);
	return retval;
}

/**
 * Dsetdrv - 14
 *
 * The GEMDOS routine Dsetdrv makes the drive drv the current default drive.
 * The following apply: drv Meaning
 */
int32_t Dsetdrv ( int16_t drv )
{
	return 1 << ('u'-'a');
}

/**
 * Dsetpath - 59
 *
 * The GEMDOS routine Dsetpath sets a new access path for the current drive.
 * The parameter path contains the new current access path for the drive.
 *
 * Note: For each drive, GEMDOS will remember the current directory. However,
 * the function Dsetpath should be used only for the current drive. For other
 * drives the following method should be applied instead: Establish current
 * drive Set desired drive Set path for this drive Make the old (remembered)
 * drive the current one again
 *
 * On some (older) versions of GEMDOS, a too frequent setting of non-existing
 * paths can lead to disturbance in the internal structures.
 *
 * int16_t Dsetpath ( CONST int8_t *path )
 */
int16_t Dsetpath ( emuptr32_t path )
{
	NOT_IMPLEMENTED(GEMDOS, Dsetpath, 59);
	return TOS_ENOSYS;
}

/**
 * Dwritelabel - 339
 *
 * The function Dwritelabel writes the name specified in label to the
 * file-system specified in path.
 *
 * Prior to MiNT version 1.14.5 this call contained a bug that allowed the
 * label to be changed by all users.
 *
 * int32_t Dwritelabel ( CONST int8_t *path, CONST int8_t *label )
 */
int32_t Dwritelabel ( emuptr32_t path, emuptr32_t label )
{
	NOT_IMPLEMENTED(GEMDOS, Dwritelabel, 339);
	return TOS_ENOSYS;
}

/**
 * Dxopendir - 1296
 *
 * Dxopendir calls the normal Dopendir, but then saves the returned handle
 * and flag in a chained list, so that Dreaddir and Dxreaddir know what they
 * have to convert for this directory. Dcloseddir deletes the handle again
 * from the list.
 *
 * Dreaddirand Dxreaddir convert the returned name to lower case.
 *
 * The function opens the directory name for reading. The parameter flag
 * describes the manner how the directory is to be opened: flag Meaning
 *
 * int32_t Dxopendir ( int8_t *name, int16_t flag )
 */
int32_t Dxopendir ( emuptr32_t name, int16_t flag )
{
	NOT_IMPLEMENTED(GEMDOS, Dxopendir, 1296);
	return TOS_ENOSYS;
}

/**
 * Dxreaddir - 322
 *
 * The function Dxreaddir returns the next file from the directory with the
 * handle dirh. The filename and the optional 4 byte long file index are
 * stored in the buffer specified in buf.
 *
 * The file index is omitted if the compatibility mode was specified for
 * Dopendir. If two filenames have the same index, they represent the same
 * file.
 *
 * MagiCreturns for FAT file-systems the following index data: Directories:
 * Start cluster in Motorola format Other files: High word = Start cluster of
 * the directory
 *
 * int32_t Dxreaddir ( int16_t ln, int32_t dirh, int8_t *buf, XATTR *xattr, int32_t *xr )
 */
int32_t Dxreaddir ( int16_t ln, int32_t dirh, emuptr32_t buf, emuptr32_t xattr, emuptr32_t xr )
{
	NOT_IMPLEMENTED(GEMDOS, Dxreaddir, 322);
	return TOS_ENOSYS;
}
