/*
	Glidix kernel

	Copyright (c) 2014-2015, Madd Games.
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
#include <glidix/procmem.h>
#include <stddef.h>

#define	SEEK_SET			0
#define	SEEK_END			1
#define	SEEK_CUR			2

/**
 * Extra flags for st_mode (other than the permissions).
 */
#define	VFS_MODE_SETUID			04000
#define	VFS_MODE_SETGID			02000
#define	VFS_MODE_STICKY			01000

#define	VFS_MODE_REGULAR		0		/* 0, so default */
#define	VFS_MODE_DIRECTORY		010000
#define	VFS_MODE_CHARDEV		020000
#define	VFS_MODE_BLKDEV			030000
#define	VFS_MODE_FIFO			040000
#define	VFS_MODE_LINK			050000		/* soft link */

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
#define	VFS_LINK_LOOP			-8		/* symbolic link depth too high */
#define	VFS_IO_ERROR			-9		/* other, unknown problem (e.g. readlink failed) */

/**
 * Flags to parsePath()/vfsOpen().
 */
#define	VFS_CHECK_ACCESS		(1 << 0)
// if set, and parsePath() finds an empty directory while searching, it will return the end directory pointer
// for it, and set *error to VFS_EMPTY_DIRECTORY.
#define	VFS_STOP_ON_EMPTY		(1 << 1)
// if set, parsePath() will create a regular file if it does not exist.
#define	VFS_CREATE			(1 << 2)
/* next 12 bits reserved */
// if final component is a symlink, do not follow
#define	VFS_NO_FOLLOW			(1 << 14)

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

#define	FILENAME_MAX			128
#define	PATH_MAX			256
#define	VFS_MAX_LINK_DEPTH		8

#ifndef _SYS_STAT_H
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
#endif

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
	 * oflag used to open the file.
	 */
	int oflag;

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
	 */
	int (*ioctl)(struct _File *file, uint64_t cmd, void *argp);

	/**
	 * Get the file state. Returns 0 on success, -1 on failure. If this is NULL then fstat() on
	 * this file description always fails, which is a bad thing. If -1 is returned or this function
	 * pointer is NULL, then EIO is returned to the caller.
	 */
	int (*fstat)(struct _File *file, struct stat *st);

	/**
	 * Like chmod() in DIR, except takes a file description instead of a directory pointer.
	 */
	int (*fchmod)(struct _File *file, mode_t mode);

	/**
	 * Like chown() in DIR, except takes a file description instead of a directory pointer.
	 */
	int (*fchown)(struct _File *file, uid_t uid, gid_t gid);

	/**
	 * Synchronise file contents with the disk. This function shall wait until all data modified in the
	 * file has been written to disk. It cannot fail.
	 */
	void (*fsync)(struct _File *file);

	/**
	 * Truncate the file to the specified size. If the size is actually larger than the current size, then
	 * zero-pad the file until it is the specified size. This can never fail if defined.
	 */
	void (*truncate)(struct _File *file, off_t length);

	/**
	 * Create a FrameList that can be used to read/write from the file - this is used by mmap().
	 * If NULL is returned, then the operation failed and mmap() returns with error code ENODEV.
	 * The returned FrameList should be such that pdownref() can be called on it aftewards, without
	 * causing it to delete. For example, it could be returned by palloc().
	 */
	FrameList* (*mmap)(struct _File *file, size_t len, int prot, int flags, off_t offset);
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
	 * 'walkflags' should be inherited, if supported!
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

	/**
	 * Change the bottom 12 bits of st_mode, and save that change to disk. Do not perform any security
	 * checks. If the change cannot be saved, then this function shall return -1; else the function
	 * shall return 0. Please note that the top 4 bits of mode shall be ignored and the top 4 bits of
	 * st_mode shall remain unchanged!
	 */
	int (*chmod)(struct _Dir *dir, mode_t mode);

	/**
	 * Change the value of st_uid and st_gid, and save that change to disk. Do not perform any security
	 * checks. If the change cannot be saved, then this function shall return -1; else the function
	 * shall return 0.
	 */
	int (*chown)(struct _Dir *dir, uid_t uid, gid_t gid);

	/**
	 * Create a new directory. 'dir' is guaranteed to be a directory end pointer (i.e. the state of a DIR
	 * structure after a call to next() that returned -1). This function shall add a new directory entry
	 * to 'dir', which shall represent a directory with name 'name' and access mode 'mode'. It is guaranteed
	 * that this directory does not already contain an entry with the specified name. It is also guaranteed
	 * that 2 mkdir() (or other creation or removal) requests do not happen at the same time.
	 */
	int (*mkdir)(struct _Dir *dir, const char *name, mode_t mode, uid_t uid, gid_t gid);

	/**
	 * Create a new regular file. 'dir' is guaranteed to be a directory end pointer. This function shall add
	 * a new directory entry to 'dir', which shall represent a new file with name 'name', and have the specified
	 * mode, owner and group. It is guaranteed that this direcotry does not already contain an entry with the
	 * specified name, and 2 mkreg() (or other creation or removal) requests do not happen at the same time.
	 * Return -1 on failure, or 0 on success; if successful, 'dir' shall be modified to point to this new file.
	 */
	int (*mkreg)(struct _Dir *dir, const char *name, mode_t mode, uid_t uid, gid_t gid);

	/**
	 * Remove the directory entry pointed to by this directory pointer. If the entry is the only link to the inode
	 * left, then the inode is also removed, along with all its contents. If the file is currently open, then the
	 * inode shall be removed after all file desctipions referring to it are closed. It is guaranteed that 2 calls
	 * to unlink() or any other creation/deletion function do not happen at the same time. Return 0 on success, or
	 * -1 on error. This function will only be called on empty directories; the kernel will assume that a directory
	 * is empty if st_size is 0.
	 */
	int (*unlink)(struct _Dir *dir);

	/**
	 * Create a hard link in this directory to an already-existing file. 'dir' is guaranteed to be a directory end pointer.
	 * It is guranteed that this directory does not already contain an entry with the given name, and 2 link() (or other
	 * creation or removal) requests do not happen at the same time. Return -1 on error (EIO) or 0 on success.
	 * 'ino' is guranteed to be the inode number of a file on the same filesystem (i.e. with st_dev matching that of this
	 * directory).
	 */
	int (*link)(struct _Dir *dir, const char *name, ino_t ino);

	/**
	 * Create a symbolic/soft link with the specified name, and store the specified path. See mkreg() for more info on file-creation
	 * semantics. Returns 0 on success, or -1 on error (EIO).
	 */
	int (*symlink)(struct _Dir *dir, const char *name, const char *path);

	/**
	 * Read the path of a symbolic link pointed to by this directory pointer. The path shall be stored in 'buffer', which
	 * has a length of at least PATH_MAX. On failure, return -1; otherwise, return the actual size of the path, which must
	 * be smaller than PATH_MAX. Also, the path must be NUL-terminated.
	 */
	ssize_t (*readlink)(struct _Dir *dir, char *buffer);

	/**
	 * Used to implement efficient stat(). If this function is NULL, then it is expected that opendir(), openroot() and
	 * next() will automatically set the 'stat' field to a valid value. If this function IS implemented, then the stat
	 * field should only be filled in when this is called.
	 */
	void (*getstat)(struct _Dir *dir);
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
	 * Return 0 on success, -1 on error (reported as EBUSY).
	 */
	int (*unmount)(struct _FileSystem *fs);

	/**
	 * The name of this filesystem.
	 */
	const char *fsname;
	
	/**
	 * Device ID, assigned during mount.
	 */
	dev_t dev;
} FileSystem;

void dumpFS(FileSystem *fs);
int vfsCanCurrentThread(struct stat *st, mode_t mask);

char *realpath(const char *relpath, char *buffer);

/**
 * Construct the Dir such that it points to the file of the specified path. Returns NULL on error.
 * If the pathname ends with a slash ('/'), then the returned Dir is the actual directory opened,
 * i.e. it's the first entry in the specified directory.
 */
Dir *parsePath(const char *path, int flags, int *error);

int vfsStat(const char *path, struct stat *st);
int vfsLinkStat(const char *path, struct stat *st);
File *vfsOpen(const char *path, int flags, int *error);
ssize_t vfsRead(File *file, void *buffer, size_t size);
ssize_t vfsWrite(File *file, const void *buffer, size_t size);
void vfsClose(File *file);

/**
 * This is used to ensure that 2 or more threads do not try to create files/directories simultaneously.
 */
void vfsInit();
void vfsLockCreation();
void vfsUnlockCreation();

#endif
