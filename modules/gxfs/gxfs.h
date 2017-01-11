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

#ifndef GXFS_H_
#define GXFS_H_

#include <glidix/common.h>
#include <glidix/semaphore.h>
#include <glidix/vfs.h>

#define	GXFS_MAGIC			(*((const uint64_t*)"GLIDIXFS"))

typedef struct
{
	uint64_t			sbMagic;
	uint8_t				sbMGSID[16];
	int64_t				sbFormatTime;
	uint64_t			sbTotalBlocks;
	uint64_t			sbChecksum;
	uint64_t			sbUsedBlocks;
	uint64_t			sbFreeHead;
	uint8_t				sbPad[512-0x40];
} GXFS_Superblock;

typedef struct
{
	uint16_t			inoOwner;
	uint16_t			inoGroup;
	uint16_t			inoMode;
	uint16_t			inoTreeDepth;
	uint64_t			inoLinks;
	uint64_t			inoSize;
	int64_t				inoBirthTime;
	int64_t				inoChangeTime;
	int64_t				inoModTime;
	int64_t				inoAccessTime;
	uint32_t			inoBirthNano;
	uint32_t			inoChangeNano;
	uint32_t			inoModNano;
	uint32_t			inoAccessNano;
	uint64_t			inoIXPerm;
	uint64_t			inoOXPerm;
	uint64_t			inoDXPerm;
	uint64_t			inoRoot;
	uint64_t			inoACL;
	char				inoPath[256];
	uint8_t				inoPad[512-0x170];
} GXFS_Inode;

typedef struct
{
	uint64_t			ptrTable[64];
} GXFS_Indirection;

typedef struct
{
	uint64_t			deInode;
	uint8_t				deOpt;
	char				name[];
} GXFS_Dirent;

struct GXFS_;

/**
 * In-memory directory entry information. When we open a directory, its listing is loaded into
 * memory as a linked list of those structures, and stored in the inode information object (described
 * below) for the directory. When the inode is closed, and the directory listing has been updated,
 * it is then written to the inode.
 */
typedef struct DirentInfo_
{
	/**
	 * If this is NULL, then we are at the "directory end pointer"; 'ent' shall be completely
	 * ignored.
	 */
	struct DirentInfo_*		next;
	
	/**
	 * The deOpt field.
	 */
	uint8_t				opt;
	
	/**
	 * The entry; if d_ino is zero it means it was unlinked.
	 */
	struct dirent			ent;
} DirentInfo;

/**
 * In-memory inode information. This forms a list in the GXFS structure, and stores each
 * open inode.
 */
typedef struct InodeInfo_
{
	struct InodeInfo_*		prev;
	struct InodeInfo_*		next;
	
	/**
	 * The filesystem to which this inode belongs.
	 */
	struct GXFS_*			fs;
	
	/**
	 * Reference count - the number of times this inode is being used. This is not the same
	 * as the number of times it was OPENED - that is included in the inoLinks field.
	 * When the refcount drops to 0, the inode is removed from the list and saved to disk.
	 * Furthermore, if the refcount drops to 0, and inoLinks is 0 at that time, the inode
	 * and all its data on disk are freed (the file/directory is deleted).
	 */
	int				refcount;
	
	/**
	 * Inode/block number.
	 */
	uint64_t			index;
	
	/**
	 * Set to 1 if anything in the inode's data has changed.
	 */
	int				dirty;
	
	/**
	 * The inode data from disk. If this is a directory, then only dir.c code is allowed to
	 * touch this!
	 */
	GXFS_Inode			data;
	
	/**
	 * Mutual exclusion lock. Use for everything EXCEPT:
	 *  - setting dirty to 1
	 *  - updating inoLinks (use atomic operations for that)
	 *  - updating refcount (use atomic operations for that)
	 *  - updating recorded times
	 */
	Semaphore			lock;
	
	/**
	 * Head of dirent list (or NULL if not a directory), and directory lock. The lock shall be used
	 * to initialize the list (lock, check if NULL and if so load, unlock) and for no other reason.
	 * The writing is performed when refcount is 0.
	 */
	Semaphore			semDir;
	int				dirDirty;
	DirentInfo*			dir;
} InodeInfo;

typedef struct GXFS_
{
	File*				fp;
	GXFS_Superblock			sb;
	
	/**
	 * Inode list.
	 */
	Semaphore			semInodes;
	InodeInfo*			firstIno;
	InodeInfo*			lastIno;
	
	/**
	 * Mutex for the allocator.
	 */
	Semaphore			semAlloc;
} GXFS;

/* === block.c === */

/**
 * Allocate a free block, and return its index. Returns 0 if we are out of space.
 */
uint64_t gxfsAllocBlock(GXFS *gxfs);

/**
 * Free a block.
 */
void gxfsFreeBlock(GXFS *gxfs, uint64_t index);

/**
 * Zero out the specified block.
 */
void gxfsZeroBlock(GXFS *gxfs, uint64_t block);

/* === inode.c === */

/**
 * Get an InodeInfo given an inode number. NULL is returned in case of I/O error. This uprefs the inode,
 * so remember to call gxfsDownrefInode() later.
 */
InodeInfo* gxfsGetInode(GXFS *gxfs, uint64_t ino);

/**
 * Flush the inode.
 */
void gxfsFlushInode(InodeInfo *info);

/**
 * Downref an inode.
 */
void gxfsDownrefInode(InodeInfo *info);

/**
 * Read from an inode at the specified position.
 */
ssize_t gxfsReadInode(InodeInfo *info, void *buffer, size_t size, off_t pos);

/**
 * Resize an inode. Return 0 on success, -1 on error (size too large).
 */
int gxfsResize(InodeInfo *info, size_t newSize);

/**
 * Write to an inode at the specified position.
 */
ssize_t gxfsWriteInode(InodeInfo *info, const void *buffer, size_t size, off_t pos);

/* === dir.c === */

/**
 * Open a directory, and fill out the scanner. Returns 0 on success, VFS error number on error.
 */
int gxfsOpenDir(GXFS *gxfs, uint64_t ino, Dir *dir, size_t szdir);

/**
 * Flush a directory.
 */
void gxfsFlushDir(InodeInfo *info);

/* === file.c === */

/**
 * Open the specified inode as a file and fill out the File structure. Returns 0 on success or -1 on error.
 */
int gxfsOpenFile(GXFS *gxfs, uint64_t ino, File *fp, size_t filesz);

#endif
