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

#ifndef __glidix_ftree_h
#define __glidix_ftree_h

#include <glidix/common.h>
#include <glidix/semaphore.h>
#include <glidix/rlock.h>

/**
 * Tree flags.
 */
#define	FT_ANON					(1 << 0)
#define	FT_READONLY				(1 << 1)
#define	FT_FIXED_SIZE				(1 << 2)

/**
 * Describes a single node on a file page tree. There are 16 entries, indexed by each 4
 * bits in a file offset. The bottom level specifies the physical page number.
 */
typedef union FileNode_
{
	union FileNode_*			nodes[16];
	uint64_t				entries[16];
} FileNode;

/**
 * Describes the page structure of a file. It's like a page table except it refers to
 * the contents of a file.
 */
typedef struct FileTree_
{
	/**
	 * Links, to allow file trees to form a linked list.
	 */
	struct FileTree_*			prev;
	struct FileTree_*			next;
	
	/**
	 * Reference count. If it is zero, the file tree is cached, and parts of it may be released
	 * for other applications to use.
	 */
	int					refcount;
	
	/**
	 * Tree flags.
	 */
	int					flags;
	
	/**
	 * Lock used to access the nodes.
	 */
	Semaphore				lock;
	
	/**
	 * Arbitrary filesystem driver information.
	 */
	void*					data;
	
	/**
	 * Function pointer set by the driver, to read a specific page into a buffer.
	 * The passed offset is always page-aligned. Returns 0 on success, -1 on error.
	 *
	 * The buffer passed in is initialized to all zeroes; if the size read is below
	 * the page size, the rest of the buffer must be left zeroed out.
	 */
	int (*load)(struct FileTree_ *ft, off_t pos, void *buffer);
	
	/**
	 * Function pointer set by the driver, to flush a specific page into the file.
	 * The passed offset is always page-aligned. Returns 0 on success, -1 on error.
	 */
	int (*flush)(struct FileTree_ *ft, off_t pos, const void *buffer);
	
	/**
	 * Function pointer set by the driver, called whenever the file is resized by ftWrite().
	 */
	void (*update)(struct FileTree_ *ft);
	
	/**
	 * Return a frame number for the specified position. If this is not NULL, it overrides the
	 * normal behaviour, and no tree is actually in use. 'pos' is page-aligned. The returned page
	 * will be marked as cached in pageinfo, and will never be uncached or freed. Return 0 if no
	 * page exists.
	 */
	uint64_t (*getpage)(struct FileTree_ *ft, off_t pos);
	
	/**
	 * Top-level node.
	 */
	FileNode				top;
	
	/**
	 * Current size of this file in bytes.
	 */
	size_t					size;
	
	/**
	 * Record lock.
	 */
	RangeLock				rlock;
} FileTree;

/**
 * Initialize the file tree subsystem.
 */
void ftInit();

/**
 * Create a new file tree with an initial reference count of 1 and place it on the list.
 * After the tree is returned, the driver may set callbacks.
 */
FileTree* ftCreate(int flags);

/**
 * Increase the reference count of the specified tree.
 */
void ftUp(FileTree *ft);

/**
 * Decrease the reference count of the specified tree. If the tree is anoynouse (FT_ANON), it is deleted
 * but the pages are NOT released (the pages are marked non-cached so they should already be deleted).
 */
void ftDown(FileTree *ft);

/**
 * Get a page mapping. If the requested offset (guaranteed to be page aligned) has not been loaded into
 * memory, it gets loaded now. Returns the physical page NUMBER on success, or 0 on error. The returned
 * frame has its reference count incremented.
 */
uint64_t ftGetPage(FileTree *ft, off_t pos);

/**
 * Commit the contents of the file tree to disk.
 */
void ftFlush(FileTree *ft);

/**
 * Read data from a file tree at the specified position.
 */
ssize_t ftRead(FileTree *ft, void *buffer, size_t size, off_t pos);

/**
 * Write data to a file tree at the specified position.
 */
ssize_t ftWrite(FileTree *ft, const void *buffer, size_t size, off_t pos);

/**
 * Change the size of the specified file tree. This is called by truncate() implementations on regular files,
 * aside from updating the inode. If the file has become smaller, the out-of-range pages are uncached.
 * Returns 0 on success; -1 if the tree is read-only.
 */
int ftTruncate(FileTree *ft, size_t size);

/**
 * Uncache the file tree. This removes it from the global list of file trees and converts it to anonymouse. It must
 * have at least one reference! If a subsequent ftDown() decreases the refcount to 0, the tree is deleted.
 */
void ftUncache(FileTree *ft);

/**
 * Flush one of the pages of a file tree, and return the frame number, which can now be reused (but is not freed).
 * Return 0 if finding a spare page was unsuccessful.
 */
uint64_t ftGetFreePage();

/**
 * Release all record locks owned by the current process on the given file.
 */
void ftReleaseProcessLocks(FileTree *ft);

/**
 * Set a file lock on the given tree. 'block' determines whether blocking is allowed. Returns 0 on success, or an
 * error number (errno) on error. The lock is associated with the current process.
 */
int ftSetLock(FileTree *ft, int type, uint64_t start, uint64_t size, int block);

/**
 * Get a file lock on the given tree.
 */
void ftGetLock(FileTree *ft, int *type, int *pidOut, uint64_t *start, uint64_t *size);

#endif
