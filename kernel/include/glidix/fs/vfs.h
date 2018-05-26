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

#include <glidix/util/common.h>
#include <glidix/thread/procmem.h>
#include <glidix/thread/mutex.h>
#include <glidix/fs/ftree.h>
#include <stddef.h>

#define	VFS_SEEK_SET			0
#define	VFS_SEEK_END			1
#define	VFS_SEEK_CUR			2

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
#define	VFS_MODE_LINK			0x5000		/* symlink */
#define	VFS_MODE_SOCKET			0x6000

/**
 * Mode type mask.
 */
#define	VFS_MODE_TYPEMASK		0xF000

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
#define	VFS_INODE_NOUNLINK		(1 << 1)		/* inode cannot be unlinked */

/**
 * Dentry flags.
 */
#define	VFS_DENTRY_TEMP			(1 << 0)		/* do not commit to disk */
#define	VFS_DENTRY_MNTPOINT		(1 << 1)		/* dirent is a mountpoint */

/**
 * Maximum depth of symbolic links.
 */
#define	VFS_MAX_LINK_DEPTH		8

/**
 * Flags for struct kstatvfs.
 */
#define	VFS_ST_RDONLY			(1 << 0)
#define	VFS_ST_NOSUID			(1 << 1)

/**
 * vfsMakeDirEx() flags.
 */
#define	VFS_MKDIR_NOVALID		(1 << 0)		/* do not validate (ignore ROFS etc) */

/**
 * Mount flags.
 */
#define	MNT_RDONLY			(1 << 0)
#define	MNT_NOSUID			(1 << 1)
#define	MNT_TEMP			(1 << 2)
#define	MNT_ALL				((1 << 3)-1)

/**
 * The AT_* flags.
 */
#define	VFS_AT_REMOVEDIR		(1 << 0)
#define	VFS_AT_SYMLINK_FOLLOW		(1 << 1)

#define	VFS_AT_NOVALID			(1 << 16)		/* do not validate (ignore ROFS) */

/**
 * File descriptor referring to the current working directory.
 */
#define	VFS_AT_FDCWD			0xFFFF

/**
 * vfsMove() flags.
 */
#define	VFS_MV_EXCL			(1 << 0)		/* do not replace 'newpath' if already exists */

/**
 * Access control list stuff.
 */
#define	VFS_ACL_SIZE			128

#define	VFS_ACE_UNUSED			0
#define	VFS_ACE_USER			1
#define	VFS_ACE_GROUP			2

#define	VFS_ACE_READ			4
#define	VFS_ACE_WRITE			2
#define	VFS_ACE_EXEC			1

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

struct kstatvfs
{
	union
	{
		struct
		{
			unsigned long	f_bsize;
			unsigned long	f_frsize;
			unsigned long	f_blocks;
			unsigned long	f_bfree;
			unsigned long	f_bavail;
			unsigned long	f_files;
			unsigned long	f_ffree;
			unsigned long	f_favail;
			unsigned long	f_fsid;
			unsigned long	f_flag;
			unsigned long	f_namemax;
			char		f_fstype[16];
			char		f_bootid[16];
		};
		
		char __size[1024];
	};
};

struct kdirent
{
	ino_t				d_ino;
	char				d_resv[64];
	char				d_name[];
};

/**
 * All the structure typedefs here, definitions below.
 */
typedef struct Inode_ Inode;
typedef struct Dentry_ Dentry;
typedef struct File_ File;
typedef struct FileSystem_ FileSystem;
typedef struct MountPoint_ MountPoint;

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
	 * prev/next links on the filesystem's inode map.
	 */
	Inode* prev;
	Inode* next;

	/**
	 * Number of file descriptions pointing at this inode. This is taken into account when
	 * unmounting filesystems, to ensure that there are no open files when a filesystem is
	 * being unmounted.
	 */
	int numOpens;
	
	/**
	 * The lock. Protects all mutable fields of this structure. It is also held when
	 * flushing. It needs to be recursive to avoid problems if, for example, rename()
	 * gets called on dentries from the same directory.
	 */
	Mutex lock;
	
	/**
	 * Specifies which filesystem this inode belongs to. This may be NULL if the
	 * inode is not on any filesystem (e.g. a socket or a pipe).
	 */
	FileSystem* fs;
	
	/**
	 * If this is a directory inode, then this points to the dentry referring to it
	 * (there is only one hard link to each directory). Used to find canonical paths
	 * and for resolving "..". This counts towards the reference count of that dentry's
	 * directory inode.
	 *
	 * If this is a symbolic link inode, this is the dentry referring to it, since symbolic
	 * links only have one hard link to them, and it is needed to follow the symlink.
	 *
	 * Do not trust this value for any other type of inode.
	 */
	Dentry* parent;
	
	/**
	 * The inode number on the filesystem. It is set to zero if the inode is dropped.
	 */
	ino_t ino;
	
	/**
	 * Access mode and inode type. The type is immutable.
	 */
	mode_t mode;
	
	/**
	 * Number of links to this inode. This is the number of hard links on the disk; 'numOpens'
	 * is for counting open file descriptors.
	 */
	nlink_t links;
	
	/**
	 * Owner and associated group.
	 */
	uid_t uid;
	gid_t gid;
	
	/**
	 * Number of blocks currently taken on disk.
	 */
	blkcnt_t numBlocks;
	
	/**
	 * File times.
	 */
	time_t atime, mtime, ctime, btime;
	
	/**
	 * Nanosecond additions to the file times.
	 */
	uint32_t anano, mnano, cnano, bnano;
	
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
	 * Next dentry key to assign.
	 */
	int nextKey;
	
	/**
	 * The file tree, if this is a random-access file (else NULL). When the file isn't open, this must
	 * have a reference count of zero, i.e. "flushed".
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
	
	/**
	 * If these function pointers are not NULL, then it is called every time this inode is opened,
	 * and may return additional data to be associated with the file description, and to release
	 * this data, respectively.
	 *
	 * If open() is implemented, it must return non-NULL on success. If it returns NULL, it must
	 * also set ERRNO, and the open is rejected in this case.
	 */
	void* (*open)(Inode *inode, int oflags);
	void (*close)(Inode *inode, void *filedata);
	
	/**
	 * For non-random-access files, implementations of pread() and pwrite().
	 *
	 * These functions, when returning -1 to indicate an error, must also set ERRNO.
	 */
	ssize_t (*pread)(Inode *inode, File *fp, void *buffer, size_t size, off_t offset);
	ssize_t (*pwrite)(Inode *inode, File *fp, const void *buffer, size_t size, off_t offset);
	
	/**
	 * For files which support this, ioctl() implementation. 'fp' is the file description being operated
	 * on. See <glidix/ioctl.h> for information on how commands and ioctl() in general work.
	 *
	 * This function may set ERRNO.
	 */
	int (*ioctl)(Inode *inode, File *fp, uint64_t cmd, void *argp);
	
	/**
	 * Flush this inode to disk. Return 0 on success, or -1 on error (and set ERRNO).
	 * If NULL, the filesystem is virtual and flushing is always reported successful.
	 */
	int (*flush)(Inode *inode);
	
	/**
	 * Drop the inode - mark it as free on the disk. This is called when there are no
	 * more links to the inode.
	 */
	void (*drop)(Inode *inode);
	
	/**
	 * Get pollable event information. The 'sems' array is indexed by PEI_* macros, and all entries
	 * are initialized to "always ready". It should set, for each event, a semaphore which becomes non-zero
	 * when the event occurs. For example, if the semaphore in PEI_READ becomes nonzero, then the file
	 * is considered ready for reading (a read would not block). The waiting is done by semPoll().
	 */
	void (*pollinfo)(Inode *inode, File *fp, struct Semaphore_ **sems);
	
	/**
	 * This is called just before the inode is released from the cache. You may perform cleanup such as
	 * releasing 'fsdata'.
	 */
	void (*free)(Inode *inode);
	
	/**
	 * This is called by stat() if there is no file tree. If this function pointer is NULL, the size is
	 * reported as 0; if is defined, then this function shall return the size to be reported.
	 */
	size_t (*getsize)(Inode *inode);
	
	/**
	 * Duplicate counter. This is the number of times the inode has been called in.
	 */
	int dups;

	/**
	 * For files which support this, pathctl() implementation. 'inode' is the inode being operated on.
	 * pathctl() commands share space with ioctl() commands, so see <glidix/ioctl.h>.
	 *
	 * This function may set ERRNO.
	 */
	int (*pathctl)(Inode *inode, uint64_t cmd, void *argp);
};

/**
 * Describes a directory entry. Fields are protected by the directory inode lock. This does not need to be
 * reference counted - the only case there would be multiple references to the same dentry without the directory
 * being locked is if the dentry points to a directory (e.g. it's a mountpoint, the directory is open, etc), and
 * it'll only be deleted if its target inode has no references other than this one.
 */
struct Dentry_
{
	/**
	 * Links. Those are both NULL for the special "kernel root dentry".
	 */
	Dentry*					prev;
	Dentry*					next;
	
	/**
	 * Name of the entry. On the heap; create with kmalloc() or strdup(), release with kfree().
	 */
	char*					name;
	
	/**
	 * The containing directory inode. This counts as a reference of the inode.
	 */
	Inode*					dir;
	
	/**
	 * Inode number of the target.
	 */
	ino_t					ino;
	
	/**
	 * A unique "key" assigned to each dirent. It is unique within each directory, not globally.
	 * Used for race-free directory reading from userspace.
	 */
	int					key;
	
	/**
	 * The target inode if already cached (else NULL). If uncached, the FileSystem's loadInode()
	 * function pointer is used to retrieve it. For mountpoints, this points to the root directory.
	 */
	Inode*					target;
	
	/**
	 * Dentry flags (VFS_DENTRY_*).
	 */
	int					flags;
};

/**
 * Represents a mounted filesystem.
 */
struct FileSystem_
{
	/**
	 * Driver-specific data.
	 */
	void*					fsdata;
	
	/**
	 * Number of mount points at which this filesystem is mounted.
	 */
	int					numMounts;
	
	/**
	 * Filesystem ID.
	 */
	dev_t					fsid;

	/**
	 * Filesystem type. Constant string, somewhere in the driver's memory.
	 */
	const char*				fstype;
	
	/**
	 * The lock, for loading inodes and unmounting and stuff, and for the inode map.
	 */
	Semaphore				lock;
	
	/**
	 * The inode map - a linked list of all inodes loaded by this filesystem. This is used to look
	 * up indes by number, to allow support for multiple hard links to the same inode. The entries
	 * in this list DO NOT count as references to the inode; once the reference count of an inode
	 * drops to zero, the filesystem lock will be acquired, and if inbetween the refcount dropping
	 * to zero and the lock being acquired a thread tries to read it off this list, it will notice
	 * that the refcount is zero, and will leave it alone.
	 */
	Inode*					imap;
	
	/**
	 * This is called when the filesystem has been unmounted and is now detached from the VFS.
	 * You may perform cleanup, such as freeing 'fsdata'.
	 */
	void (*unmount)(FileSystem *fs);
	
	/**
	 * Load the inode with the given number (inode->ino). This function shall fill out the metadata
	 * in the inode. The refcount, lock, fs pointer, etc are all initialized by the kernel.
	 * All other fields are initially zeroed out. Return 0 on error, or -1 if an I/O error occured.
	 */
	int (*loadInode)(FileSystem *fs, Inode *inode);
	
	/**
	 * Register an inode. The 'inode' structure is filled in with information about a new inode.
	 * This function shall set its 'ino' field to the new inode number allocated on disk. Return 0
	 * on success, or -1 on error, and set ERRNO.
	 */
	int (*regInode)(FileSystem *fs, Inode *inode);
	
	/**
	 * Size of a block on this filesystem. Zero means unknown.
	 */
	uint64_t blockSize;
	
	/**
	 * Total number of blocks available on this filesystem. Zero means unknown.
	 */
	uint64_t blocks;
	
	/**
	 * Number of free blocks on this filesystem.
	 */
	uint64_t freeBlocks;
	
	/**
	 * Total number of inodes on this filesystem. Zero means unknown.
	 */
	uint64_t inodes;
	
	/**
	 * Number of free inodes on this filesystem.
	 */
	uint64_t freeInodes;
	
	/**
	 * Filesystem flags.
	 */
	unsigned long flags;
	
	/**
	 * Maximum length of a file name on this filesystem. Zero means unlimited.
	 */
	uint64_t maxnamelen;
	
	/**
	 * Boot ID of this filesystem.
	 */
	uint8_t bootid[16];
	
	/**
	 * Set to 1 if currently unmounting.
	 */
	int unmounting;
};

/**
 * Describes a mountpoint traversed in path resolution.
 */
struct MountPoint_
{
	/**
	 * The previously-traversed mountpoint. NULL for the kernel root.
	 */
	MountPoint*				prev;
	
	/**
	 * The mountpoint dentry. This counts towards its directory's reference count, but is not
	 * locked. Lock before use.
	 */
	Dentry*					dent;
	
	/**
	 * Root inode of the mounted filesystem. This counts towards the reference count.
	 */
	Inode*					root;
};

/**
 * A "dentry pointer". Specifies the dentry and the stack of mountpoints used to reach it (to help
 * resolve ".." on roots).
 */
typedef struct
{
	/**
	 * The dentry we are pointing to. It is locked by us.
	 */
	Dentry*					dent;
	
	/**
	 * Top of the mountpoint stack used to reach this.
	 */
	MountPoint*				top;
} DentryRef;

/**
 * Inode pointer. Same principle, but "top" might be NULL if the inode is neither a directory nor a
 * symbolic link.
 */
typedef struct
{
	Inode*					inode;
	MountPoint*				top;
} InodeRef;

/**
 * Null references.
 */
extern DentryRef VFS_NULL_DREF;
extern InodeRef VFS_NULL_IREF;

/**
 * Represents an open file description.
 */
struct File_
{
	/**
	 * The inode we are operating on.
	 */
	InodeRef				iref;
	
	/**
	 * The lock.
	 */
	Semaphore				lock;
	
	/**
	 * File data as returned by the inode's open(), or NULL if not implemented.
	 */
	void*					filedata;
	
	/**
	 * Current file offset.
	 */
	off_t					offset;
	
	/**
	 * Open file flags.
	 */
	int					oflags;
	
	/**
	 * Reference count.
	 */
	int					refcount;
};

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

/**
 * Initialize the VFS. This initializes locks, and also creates the kernel root inode.
 */
void vfsInit();

/**
 * Create a new filesystem. A filesystem ID is automatically assigned. 'fstype' is the name of
 * the filesystem, for example "gxfs". After calling this function, you must set all necessary
 * callbacks in the new filesystem. The new filesystem will have numMounts set to zero, and so
 * must be mounted somewhere.
 */
FileSystem* vfsCreateFileSystem(const char *fstype);

/**
 * Create a new inode of the given type and mode. It starts with a refcount of 1, and
 * link count of 0. The general procedure is as follows:
 *
 *	1. Create the inode with vfsCreateInode().
 *	2. If needed, change parameters from defaults.
 *	3. Create a hard link to the inode (if applicable).
 *	4. Remove your own reference once ready.
 *
 * On error returns NULL and sets ERRNO.
 */
Inode* vfsCreateInode(FileSystem *fs, mode_t mode);

/**
 * Upref and downref inodes.
 */
void vfsUprefInode(Inode *inode);
void vfsDownrefInode(Inode *inode);

/**
 * Upref and downref filesystems.
 */
void vfsUprefFileSystem(FileSystem *fs);
void vfsDownrefFileSystem(FileSystem *fs);

/**
 * Read the target of a symbolic link. This revokes your reference to the symlink inode
 * (decrefs it) regardless of whether or not it succeeded. This temporarily locks the
 * inode.
 *
 * The returned string is a copy on the heap, and you must call kfree() on it at some point.
 *
 * Returns NULL on I/O error.
 */
char* vfsReadLink(Inode *link);

/**
 * Get the parent dentry of the given inode. Returns NULL in 'dent' if the inode is not a
 * directory (and hence has no parent dentry). The parent inode is then locked.
 *
 * This function locks the inode temporarily, so do not call it while holding the lock.
 *
 * The directory inode of the returned entry is upreffed, and the passed-in inode is
 * decreffed. If NULL is returned, the passed-in inode is still decreffed.
 */
DentryRef vfsGetParentDentry(InodeRef inode);

/**
 * Return the directory containing the given dentry, and unlock the inode, without updating
 * the refernce count of the inode. That is, you are withdrawing your reference to the dentry,
 * and taking a reference to its directory.
 */
InodeRef vfsGetDentryContainer(DentryRef dent);

/**
 * Get the inode pointed to by the given dentry, loading it into memory if necessary.
 * This unlock the dentry's directory inode, and revokes the reference, while returning a new
 * reference to the target inode.
 *
 * If the inode could not be loaded, NULL is returned, but the reference is still revoked and
 * hence lock removed. This error should be trated as EIO (I/O error).
 */
InodeRef vfsGetDentryTarget(DentryRef dref);

/**
 * Get an inode given its dentry. If 'follow' is 1, then symbolic links are followed; otherwise
 * they're not. The reference is revoked whether successful or not.
 */
InodeRef vfsGetInode(DentryRef dent, int follow, int *error);

/**
 * Mark an inode as dirty, and perhaps flush it. Call this only when the inode is locked!
 */
void vfsDirtyInode(Inode *inode);

/**
 * Flush an inode. This commits it to disk, along with all of its data. Call this when the inode
 * is NOT locked, as this function locks the inode. Returns 0 on success, or an error number on
 * error. Flushing errors typically cannot be recovered from.
 */
int vfsFlush(Inode *inode);

/**
 * Return the inode representing the current root directory, and upref it. You must call
 * vfsDownrefInode() on this when done.
 */
InodeRef vfsGetRoot();

/**
 * Return the inode representing the current working directory, and upref it.  You must call
 * vfsDownrefInode() on this when done.
 */
InodeRef vfsGetCurrentDir();

/**
 * Get the dentry corresponding to the given name on the given directory inode. This locks the
 * inode and returns the named dentry. The dentry becomes your reference to the inode - a dentry
 * call will revoke your reference while unlocking the inode. If this function fails, NULL is
 * returned, and the reference revoked.
 *
 * If 'create' is nonzero, the dentry is created if it doesn't yet exist. In this case, the 'ino' field
 * will be 0. It will also have the VFS_DENTRY_TEMP flag set, so won't be committed to disk. Other
 * functions must be used to assign an inode and commit the dentry to disk.
 */
DentryRef vfsGetChildDentry(InodeRef dirnode, const char *entname, int create);

/**
 * Append a dentry to the inode. This is called when loading a directory inode from disk, by the filesystem
 * driver.
 */
void vfsAppendDentry(Inode *dir, const char *name, ino_t ino);

/**
 * Check if the specified user has the right to perform the given operations on the given inode.
 * The 'perms' argument is a bitwise-OR of the VFS_ACE_* flags required. This function does not
 * lock the inode.
 *
 * This function returns 0 if access was denied, or 1 if allowed. The reference count is unaffected.
 */
int vfsIsAllowed(Inode *inode, int perms);

/**
 * Given a path, starting relative resolutions from 'startdir' inode, get the dentry
 * corresponding to the path. The dentry will have its containing directory locked,
 * and you must call one of the dentry-handling functions to perform an operation on
 * it and unlock it. Alternatively, you may call vfsUnrefDentry() to unlock it
 * without doing anything.
 *
 * If the 'create' argument is nonzero, then the dentry is created if it does not exist (all parent
 * directories must already exist, however). In this case, the 'ino' field will contain 0.
 *
 * If the final result is a symbolic link, this function does NOT follow it (but DOES follow symbolic
 * links to directories on the way).
 *
 * On error, it returns NULL. If 'error' is non-NULL, an error number is stored there.
 *
 * The returned dentry is your reference to the inode. That is, the next operation on the
 * dentry, which decrefs it, also revokes the reference of the inode that you passed in.
 * If NULL is returned, the reference is revoked too.
 */
DentryRef vfsGetDentry(InodeRef startdir, const char *path, int create, int *error);

/**
 * Unlock a dentry and revoke the reference to its inode.
 */
void vfsUnrefDentry(DentryRef dent);

/**
 * Remove a reference to the inode and clear the reference's moutn stack.
 */
void vfsUnrefInode(InodeRef iref);

/**
 * Link a new inode to an empty dentry (where 'ino' is 0). This unlocks the dentry and revokes your
 * reference to it, but UPREFS the inserted inode. You must therefore later call vfsDownrefInode(),
 * or another reference-revoking function, on the inode.
 *
 * The inode must be on the same filesystem as the dentry, and the dentry becomes permanent (committed
 * to disk). To make a temporary entry (not necessarily on the same filesystem), use vfsBindInode().
 *
 * It is up to the caller to make sure the dentry is empty; in other cases, the kernel panics.
 */
void vfsLinkInode(DentryRef dent, Inode* target);

/**
 * Similar to vfsLinkInode(), except that the InodeRef is returned afterwards.
 */
InodeRef vfsLinkAndGetInode(DentryRef dent, Inode *target);

/**
 * Similar to vfsLinkInode(), but the dentry is not committed to disk.
 */
void vfsBindInode(DentryRef dent, Inode* target);

/**
 * Unlink the target inode of the dentry. This sets 'ino' to 0. Returns 0 on success, or an error number
 * on error (e.g. if a directory is not empty).
 *
 * There is only one supported flag so far: VFS_AT_REMOVEDIR. If it is set, this function removes only
 * directories (only possible if the directory is empty); otherwise, this function removes only
 * non-directories.
 *
 * The dentry reference is revoked and the dentry is removed.
 */
int vfsUnlinkInode(DentryRef dent, int flags);

/**
 * Remove a directory entry. This assumes that the dentry does not point to any inode; use it only to
 * cancel the addition of a dentry. This revokes the reference.
 */
void vfsRemoveDentry(DentryRef dent);

/**
 * Create a directory at the specified path. You must have write permission to the parent directory,
 * and it must already exist. On success, return 0, else return an error number such as ENOENT.
 */
int vfsMakeDir(InodeRef startdir, const char *path, mode_t mode);

/**
 * Like vfsMakeDir() but accepts some flags.
 */
int vfsMakeDirEx(InodeRef startdir, const char *path, mode_t mode, int flags);

/**
 * Mount the given inode at the given dentry. The reference to the dentry is revoked, regardless of
 * whether an error occured or not. The reference count of the inode is incremented if the mount was
 * successful, and so is its mount count. Returns 0 on success or an error number on error.
 *
 * This function performs permission checks.
 */
int vfsMount(DentryRef dref, Inode *mntroot, int flags);

/**
 * Open the named inode and return a new file handle. Symbolic links will be dereferenced. On success,
 * a file handle is returned. On error, NULL is returned, and if 'error' is not NULL, it is set to the
 * error number.
 */
File* vfsOpen(InodeRef startdir, const char *path, int oflag, mode_t mode, int *error);

/**
 * Open an inode reference - that is, create a file description around it. On success, it returns the
 * file description, and the inode reference is transferred to it. On error, returns NULL and if 'error'
 * is not NULL, sets it to the error number. Permission checking is NOT performed.
 *
 * Essentially your reference to the inode is revoked whether or not this function succeeds.
 */
File* vfsOpenInode(InodeRef iref, int oflag, int *error);

/**
 * Increase the refcount of a file description.
 */
void vfsDup(File *fp);

/**
 * Decrement the refcount of a file description.
 */
void vfsClose(File *fp);

/**
 * Implementations of pread(), read(), write(), pwrite().
 */
ssize_t vfsRead(File *fp, void *buffer, size_t size);
ssize_t vfsPRead(File *fp, void *buffer, size_t size, off_t offset);
ssize_t vfsWrite(File *fp, const void *buffer, size_t size);
ssize_t vfsPWrite(File *fp, const void *buffer, size_t size, off_t offset);

/**
 * Seek a file.
 */
off_t vfsSeek(File *fp, off_t off, int whence);

/**
 * Fill out the stat structure with information about an inode.
 */
void vfsInodeStat(Inode *inode, struct kstat *st);

/**
 * Get the stat about a path. Returns 0 on success (and 'st' is filled) or -1 on error and sets ERRNO.
 *	startdir	Starting directory for searches (or VFS_NULL_IREF for default)
 *	path		Path to the inode to retrieve information about.
 *	follow		If 1, and 'path' refers to a symbolic link, follow it; otherwise return info about
 *			the symbolic link itself.
 *	st		The struct kstat to fill in.
 */
int vfsStat(InodeRef startdir, const char *path, int follow, struct kstat *st);

/**
 * Get VFS information about an inode.
 */
void vfsInodeStatVFS(Inode *inode, struct kstatvfs *st);

/**
 * Get VFS information related to a path.
 */
int vfsStatVFS(InodeRef startdir, const char *path, struct kstatvfs *st);

/**
 * Change the mode of an inode.
 */
int vfsInodeChangeMode(Inode *inode, mode_t mode);

/**
 * Change the mode of a file.
 */
int vfsChangeMode(InodeRef startdir, const char *path, mode_t mode);

/**
 * Change the owner/group of an inode.
 */
int vfsInodeChangeOwner(Inode *inode, uid_t uid, gid_t gid);

/**
 * Change the owner/group of a file.
 */
int vfsChangeOwner(InodeRef startdir, const char *path, uid_t uid, gid_t gid);

/**
 * Truncate/expand an inode to the specified size. Returns 0 on success or error number on error.
 */
int vfsTruncate(Inode *inode, off_t size);

/**
 * Make a copy of an inode reference.
 */
InodeRef vfsCopyInodeRef(InodeRef iref);

/**
 * Change working directory. Return 0 on success, error number on error.
 */
int vfsChangeDir(InodeRef iref, const char *path);

/**
 * Change the root directory. Return 0 on success, error number on error.
 */
int vfsChangeRoot(InodeRef iref, const char *path);

/**
 * Return the absolute path to the specified dentry, as a string on the heap. Remember to free
 * it using kfree() later. This revokes the reference.
 */
char* vfsRealPath(DentryRef dref);

/**
 * Get the current working directory string, on the heap. Remember to release with kfree().
 */
char* vfsGetCurrentDirPath();

/**
 * Create a hard link to the "old" path at the "new" path. Implements linkat() from POSIX. Returns
 * 0 on success, or an error number on error.
 */
int vfsCreateLink(InodeRef oldstart, const char *oldpath, InodeRef newstart, const char *newpath, int flags);

/**
 * Create a symbolic link to the "old" path at the "new" path. Implements symlinkat() from POSIX. Returns
 * 0 on success, or an error number on error. Whatever the old path is, it is simply stored as-is in the
 * symlink inode; no checking whether the file exists is actually performed.
 */
int vfsCreateSymlink(const char *oldpath, InodeRef newstart, const char *newpath);

/**
 * Same as above but with flags.
 */
int vfsCreateSymlinkEx(const char *oldpath, InodeRef newstart, const char *newpath, int flags);

/**
 * Read the target of a symbolic link, returning it as a string on the heap. Remember to free it using kfree().
 * Returns NULL on error, and sets ERRNO.
 */
char* vfsReadLinkPath(InodeRef startdir, const char *path);

/**
 * Change the times associated with the specified inode. Performs all necessary permission checks. This implements
 * POSIX utime(), utimes(), and all the related functions. Returns 0 on success or -1 on error and sets ERRNO.
 */
int vfsInodeChangeTimes(Inode *inode, time_t atime, uint32_t anano, time_t mtime, uint32_t mnano);

/**
 * Change the times associated with the file at the specified path. Indirectly used to implement POSIX utime()
 * etc. Returns 0 on success, or -1 on error and sets ERRNO.
 */
int vfsChangeTimes(InodeRef startdir, const char *path, time_t atime, uint32_t anano, time_t mtime, uint32_t mnano);

/**
 * Unmount the filesystem from the mountpoint at the specified path. Return 0 on success or an error number on error.
 */
int vfsUnmount(const char *path, int flags);

/**
 * Returns a pointer to a semaphore which always has value 1.
 */
struct Semaphore_* vfsGetConstSem();

/**
 * Change the executable permissions on an inode. Returns 0 on success or -1 on error and sets ERRNO.
 */
int vfsInodeChangeXPerm(Inode *inode, uint64_t ixperm, uint64_t oxperm, uint64_t dxperm);

/**
 * Change the executable permissions on a path.
 */
int vfsChangeXPerm(InodeRef startdir, const char *path, uint64_t ixperm, uint64_t oxperm, uint64_t dxperm);

/**
 * Given an inode representing a directory, and a key for a dentry, return the 'struct kdirent' for it. On error,
 * returns the error number as a negative, otherwise return the size of the 'struct kdirent', and store a pointer
 * to it at *out. The pointer is on the heap; you must release it using kfree().
 *
 * Returns -ENOENT if there is no dentry for the key, but if there are dentries for higher keys (and you should try
 * them). Returns -EOVERFLOW if there is no dentry for the given key, or any higher one.
 */
ssize_t vfsReadDir(Inode *inode, int key, struct kdirent **out);

/**
 * Move an inode from one dentry to another. This is used to implement rename(). Returns 0 on success, error number
 * on error.
 */
int vfsMove(InodeRef startold, const char *oldpath, InodeRef startnew, const char *newpath, int flags);

#endif
