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

/**
 * Errors from openroot()/opendir().
 */
#define	VFS_EMPTY_DIRECTORY		-1		/* the directory was empty */

/**
 * Errors from parsePath().
 */
#define	VFS_NO_FILE			-2

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
 * Describes an open file. All of itsmember functions may be NULL.
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
	ssize_t (*write)(struct _File *file, void *buffer, size_t size);

	/**
	 * Seek to a different position in the file.
	 */
	off_t (*seek)(struct _File *file, off_t offset, int whence);

	/**
	 * Close the file. Do not free this structure! The kernel will do that for you.
	 */
	void (*close)(struct _File *file);
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

/**
 * Construct the Dir such that it points to the file of the specified path. Returns NULL on error.
 */
Dir *parsePath(const char *path, int flags);

File *vfsOpen(const char *path, int flags);
ssize_t vfsRead(File *file, void *buffer, size_t size);
void vfsClose(File *file);

#endif
