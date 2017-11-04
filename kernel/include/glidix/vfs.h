/*
	Glidix kernel

	Copyright (c) 2014-2017, Madd Games.
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
#include <glidix/ftree.h>
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
#define	VFS_MODE_DIRECTORY		0x1000
#define	VFS_MODE_CHARDEV		0x2000
#define	VFS_MODE_BLKDEV			0x3000
#define	VFS_MODE_FIFO			0x4000
#define	VFS_MODE_LINK			0x5000		/* soft link */
#define	VFS_MODE_SOCKET			0x6000

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
#define	O_CLOEXEC			(1 << 11)
#define	O_ACCMODE			(O_RDWR)
#define	O_ALL				(O_RDWR | O_APPEND | O_CREAT | O_EXCL | O_TRUNC | O_NOCTTY | O_NONBLOCK | O_CLOEXEC)

/**
 * Additional flags, cannot be passed to open().
 */
#define	O_SOCKET			(1 << 16)
#define	O_MSGQ				(1 << 17)
#define	O_TERMINAL			(1 << 18)

/**
 * File descriptor flags settable with fcntl().
 */
#define	FD_CLOEXEC			O_CLOEXEC
#define	FD_ALL				(FD_CLOEXEC)

/**
 * Event indices for _glidix_poll(). Those represent bit indices (NOT masks) into the "event"
 * bytes passed to _glidix_poll() to select which events shall be polled for on the given file.
 * Also used by the pollinfo() File callback as indices into the array of event semaphores; see
 * below.
 */
#define	PEI_READ			0
#define	PEI_WRITE			1
#define	PEI_ERROR			2
#define	PEI_HANGUP			3
#define	PEI_INVALID			4

/**
 * Like the above but converted to masks.
 */
#define	POLL_READ			(1 << PEI_READ)
#define	POLL_WRITE			(1 << PEI_WRITE)
#define	POLL_ERROR			(1 << PEI_ERROR)
#define	POLL_HANGUP			(1 << PEI_HANGUP)
#define	POLL_INVALID			(1 << PEI_INVALID)

/**
 * Inode flags.
 */
#define	VFS_INODE_DIRTY			(1 << 0)		/* inode changed since loading */

#define	VFS_MAX_LINK_DEPTH		8

#define	VFS_ACL_SIZE			128

#define	VFS_ACE_UNUSED			0
#define	VFS_ACE_USER			1
#define	VFS_ACE_GROUP			2

typedef struct
{
	uint16_t			ace_id;
	uint8_t				ace_type;
	uint8_t				ace_perms;
} AccessControlEntry;

struct kstat
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
	uint64_t			st_ixperm;
	uint64_t			st_oxperm;
	uint64_t			st_dxperm;
	time_t				st_btime;
	AccessControlEntry		st_acl[VFS_ACL_SIZE];
};

struct kdirent
{
	ino_t				d_ino;
	char				d_name[128];
};

/**
 * Describes a "system object". It's an object with a name in the filesystem, which may be
 * retrieved and stored by the kernel, and keeps a reference count and a value. This is used
 * for example for local (UNIX) sockets.
 *
 * The reference count shall be incremented when link() is called on the object, and decremented
 * when unlink() is called. When linksys() is called, it also increases the refcount. When the
 * refcount is decremented and reaches 0, the remove() pointer below is called, indicating the
 * value can be safely deleted. The structure itself is deleted by vfsSysDecref().
 *
 * The 'data' and 'remove' fields belong to the creator; everything else must be manipulated with
 * the VFS functions specified later in this file. The 'mode' field is read-only, and shall be
 * used to determine the type of object.
 */
typedef struct SysObject_
{
	/**
	 * Arbitrary data.
	 */
	void*				data;
	
	/**
	 * Called when refcount reaches zero, just before freeing this object.
	 */
	void (*remove)(struct SysObject_ *sysobj);
	
	/**
	 * Mode; including the type (used to determine what this object is, and hence the meaning
	 * of 'data'!)
	 */
	mode_t				mode;
	
	/**
	 * Reference count.
	 */
	int				refcount;
} SysObject;

/**
 * All the structure typedefs here, definitions below.
 */
typedef struct Inode_ Inode;
typedef struct Dentry_ Dentry;
typedef struct File_ File;
typedef struct FileSystem_ FileSystem;

/**
 * Describes a VFS inode.
 */
struct Inode_
{
	/**
	 * Reference count. This is the number of references to this inode which exist
	 * in memory (not necessarily on disk; "links" is for that).
	 */
	int refcount;
	
	/**
	 * Specifies which filesystem this inode belongs to. This may be NULL if the
	 * inode is not on any filesystem (e.g. a socket or a pipe).
	 */
	FileSystem* fs;
	
	/**
	 * The inode number on the filesystem.
	 */
	ino_t ino;
	
	/**
	 * Access mode and permissions.
	 */
	mode_t mode;
	
	/**
	 * Number of links to this inode. This is the number of file descriptions referring
	 * to it, plus the number of hard links on disk.
	 */
	nlink_t links;
	
	/**
	 * Owner and associated group.
	 */
	uid_t uid;
	gid_t gid;
	
	/**
	 * Block size and number of blocks currently taken on disk.
	 */
	blksize_t blockSize;
	blkcnt_t numBlocks;
	
	/**
	 * File times.
	 */
	time_t atime, mtime, ctime, btime;
	
	/**
	 * Executable permissions.
	 */
	uint64_t ixperm, oxperm, dxperm;
	
	/**
	 * Access control list.
	 */
	AccessControlEntry acl[VFS_ACL_SIZE];
	
	/**
	 * Inode flags (VFS_INODE_*).
	 */
	int flags;
	
	/**
	 * The file tree, if this is a random-access file (else NULL).
	 */
	FileTree* ft;
	
	/**
	 * Driver-specific inode info.
	 */
	void *fsdata;
	
	/**
	 * If this is a symlink, this is the target path.
	 */
	char *target;
	
	/**
	 * Head of the dentry cache, if this is a directory inode.
	 */
	Dentry* dents;
};

struct fsinfo;

/**
 * Filesystem information structure, for _glidix_fsinfo().
 */
typedef struct fsinfo
{
	dev_t					fs_dev;
	char					fs_image[256];
	char					fs_mntpoint[256];
	char					fs_name[64];
	size_t					fs_usedino;
	size_t					fs_inodes;
	size_t					fs_usedblk;
	size_t					fs_blocks;
	size_t					fs_blksize;
	uint8_t					fs_bootid[16];
	char					fs_pad[968];
} FSInfo;

/**
 * File lock description structure, matching with "struct flock" from the C library.
 */
typedef struct
{
	short int				l_type;
	short int				l_whence;
	int					l_pid;
	off_t					l_start;
	off_t					l_len;
	uint64_t				l_resv[8];
} FLock;

void dumpFS(FileSystem *fs);
int vfsCanCurrentThread(struct kstat *st, mode_t mask);

char *realpath(const char *relpath, char *buffer);

/**
 * Construct the Dir such that it points to the file of the specified path. Returns NULL on error.
 * If the pathname ends with a slash ('/'), then the returned Dir is the actual directory opened,
 * i.e. it's the first entry in the specified directory.
 */
Dir *parsePath(const char *path, int flags, int *error);

int vfsStat(const char *path, struct kstat *st);
int vfsLinkStat(const char *path, struct kstat *st);
File *vfsOpen(const char *path, int flags, int *error);
ssize_t vfsRead(File *file, void *buffer, size_t size);
ssize_t vfsWrite(File *file, const void *buffer, size_t size);
ssize_t vfsPRead(File *file, void *buffer, size_t size, off_t pos);
ssize_t vfsPWrite(File *file, const void *buffer, size_t size, off_t pos);
void vfsDup(File *file);
void vfsClose(File *file);

/**
 * This is used to ensure that 2 or more threads do not try to create files/directories simultaneously.
 */
void vfsInit();
void vfsLockCreation();
void vfsUnlockCreation();

/**
 * Returns a pointer to a semaphore which always has value 1.
 */
struct Semaphore_* vfsGetConstSem();

/**
 * Create a system object, reference count 1.
 */
SysObject* vfsSysCreate();

/**
 * Increment the reference count of a system object.
 */
void vfsSysIncref(SysObject *sysobj);

/**
 * Decrement the reference count of a system object, and delete it if it becomes zero.
 */
void vfsSysDecref(SysObject *sysobj);

/**
 * Link a system object into the filesystem under the given name. Returns 0 on success,
 * and increments the refcount. On error, returns one of the following errors (they may
 * be translated to different errno numbers depending on context):
 *
 * VFS_EXISTS - This file name already exists.
 * VFS_IO_ERROR - Operation not supported on the target filesystem.
 * VFS_PERM - Access to the directory was denied.
 * VFS_NO_FILE - Parent directory does not exist.
 * VFS_LONGNAME - Name is too long.
 * Other errors may come from parsePath().
 */
int vfsSysLink(const char *path, SysObject *sysobj);

/**
 * Get a system object at the specified location. Returns NULL if the specified path does
 * not refer to a system object. This also increments the refcount.
 */
SysObject* vfsSysGet(const char *path);

#endif
