/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	* Redistributions of source code must retain the above copyright notice, this
	  list of conditions and the following disclaimer.
	
	* Redistributions in binary form must reproduce the above copyright notice,
	  this list of conditions and the following disclaimer in the documentation
	  and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __glidix_vfs_h
#define __glidix_vfs_h

/**
 * Abstraction of file systems.
 */

#include <glidix/common.h>
#include <stddef.h>

#define	SEEK_SET			0
#define	SEEK_END			1
#define	SEEK_CUR			2

/**
 * Extra flags for st_mode (other than the permissions).
 */
#define	VFS_MODE_SETUID			(1 << 9)
#define	VFS_MODE_SETGID			(1 << 10)
#define	VFS_MODE_DIRECTORY		(1 << 11)
#define	VFS_MODE_CHARDEV		(1 << 12)
#define	VFS_MODE_BLKDEV			(1 << 13)
#define	VFS_MODE_FIFO			(1 << 14)
#define	VFS_MODE_LINK			(1 << 15)

/**
 * Errors.
 */
#define	VFS_EMPTY_DIRECTORY		-1		/* the directory was empty */
#define	VFS_BAD_FD			-2		/* bad file descriptor */
#define	VFS_PERM			-3		/* permission denied */
#define	VFS_NO_FILE			-4
#define	VFS_FILE_LIMIT_EXCEEDED		-5		/* MAX_OPEN_FILES limit exceeded */
#define	VFS_BUSY			-6		/* a file is busy */
#define	VFS_NOT_DIR			-7		/* not a directory */

/**
 * Flags to parsePath()/vfsOpen().
 */
#define	VFS_CHECK_ACCESS		(1 << 0)

/**
 * Flags for the open() system call.
 */
#define	O_WRONLY			(1 << 0)
#define	O_RDONLY			(1 << 1)
#define	O_RDWR				(O_WRONLY | O_RDONLY)
#define	O_APPEND			(1 << 2)
#define	O_CREAT				(1 << 3)
#define	O_EXCL				(1 << 4)
#define	O_NOCTTY			(1 << 5)
#define	O_TRUNC				(1 << 6)
#define	O_DSYNC				(1 << 7)
#define	O_NONBLOCK			(1 << 8)
#define	O_RSYNC				(1 << 9)
#define	O_SYNC				(1 << 10)
#define	O_ACCMODE			(O_RDWR)
#define	O_ALL				((1 << 11)-1)

struct stat
{
	dev_t				st_dev;
	ino_t				st_ino;
	mode_t				st_mode;
	nlink_t				st_nlink;
	uid_t				st_uid;
	gid_t				st_gid;
	dev_t				st_rdev;
	off_t				st_size;
	blksize_t			st_blksize;
	blkcnt_t			st_blocks;
	time_t				st_atime;
	time_t				st_mtime;
	time_t				st_ctime;
};

struct dirent
{
	ino_t				d_ino;
	char				d_name[128];
};

/**
 * Describes an open file. All of its member functions may be NULL.
 */
typedef struct _File
{
	/**
	 * Private, filesystem-specific structure.
	 */
	void *fsdata;

	/**
	 * Read the file.
	 */
	ssize_t (*read)(struct _File *file, void *buffer, size_t size);

	/**
	 * Write to the file.
	 */
	ssize_t (*write)(struct _File *file, const void *buffer, size_t size);

	/**
	 * Seek to a different position in the file.
	 */
	off_t (*seek)(struct _File *file, off_t offset, int whence);

	/**
	 * Close the file. Do not free this structure! The kernel will do that for you.
	 */
	void (*close)(struct _File *file);

	/**
	 * Duplicate the file description into another File structure. Both descriptions must
	 * be initially identical, but may become different when read(), write(), seek() and
	 * other functions are called on them. Return 0 if the duplication was successful; -1
	 * if this description cannot be duplicated for whatever reason. If this entry is NULL,
	 * the file description is considered impossible to duplicate.
	 *
	 * If a file description cannot be duplicated, the dup() and dup2() system calls return
	 * errors, and the fork() and _glidix_clone() (when creating a new file table) system calls
	 * shall make the child process see the file descriptor referring to this description as
	 * closed.
	 */
	int (*dup)(struct _File *me, struct _File *file, size_t szFile);

	/**
	 * This is used to implement the ioctl() interface. The 'cmd' argument must be constructed
	 * according to the rules from <glidix/ioctl.h>. The last argument is undefined for 0-sized
	 * commands. Return 0 on success, -1 on error. All ioctl() calls will fail with EINVAL if
	 * this entry is NULL.
	 *
	 * WARNING: Accessing the param structure could throw a #PF which causes the function to be
	 * interrupted and return the EINTR error; you must be careful with your locks!
	 */
	int (*ioctl)(struct _File *file, uint64_t cmd, void *argp);
} File;

/**
 * This structure is used to describe an open directory scanner.
 */
typedef struct _Dir
{
	/**
	 * Private, filesystem-specific structure.
	 */
	void *fsdata;

	/**
	 * Description of the current entry and its stat().
	 */
	struct dirent			dirent;
	struct stat			stat;

	/**
	 * If this entry points to a file, open the file. Return 0 on success, -1 on error.
	 * The passed File structure was initialized to default values; please only change
	 * the values that you need to. Also, the size was passed in case there is a mismatch
	 * between the target kernel version of a module and the actual kernel version.
	 */
	int (*openfile)(struct _Dir *me, File *file, size_t szFile);

	/**
	 * Same as openfile(), except opens a directory. It may return error codes as described above.
	 */
	int (*opendir)(struct _Dir *me, struct _Dir *dir, size_t szDir);

	/**
	 * Close this scanner. Do not free the Dir structure! The kernel will do that for you.
	 */
	void (*close)(struct _Dir *dir);

	/**
	 * Advance to the next entry in this directory. Return 0 on success, -1 on end of list.
	 * If this is the last entry, and the filesystem is not read-only, this structure shall
	 * remain valid so that mkdir() or mknode() can be called to add a new entry.
	 */
	int (*next)(struct _Dir *dir);
} Dir;

/**
 * Describes a mounted instance of a filesystem, perhaps attached to a storage device.
 */
typedef struct _FileSystem
{
	/**
	 * Private, filesystem-specific data structure.
	 */
	void *fsdata;

	/**
	 * Open the root directory of this filesystem. Return 0 on success, a negative number
	 * on error; see above for more info.
	 */
	int (*openroot)(struct _FileSystem *fs, Dir *dir, size_t szDir);

	/**
	 * Unmount this filesystem. Do not free this structure! The kernel does that for you.
	 */
	void (*unmount)(struct _FileSystem *fs);

	/**
	 * The name of this filesystem.
	 */
	const char *fsname;
} FileSystem;

void dumpFS(FileSystem *fs);
int vfsCanCurrentThread(struct stat *st, mode_t mask);

/**
 * Construct the Dir such that it points to the file of the specified path. Returns NULL on error.
 * If the pathname ends with a slash ('/'), then the returned Dir is the actual directory opened,
 * i.e. it's the first entry in the specified directory.
 */
Dir *parsePath(const char *path, int flags, int *error);

int vfsStat(const char *path, struct stat *st);
File *vfsOpen(const char *path, int flags, int *error);
ssize_t vfsRead(File *file, void *buffer, size_t size);
void vfsClose(File *file);

#endif
