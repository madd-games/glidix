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

#ifndef __glidix_ftab_h
#define __glidix_ftab_h

/**
 * A structure that describes a file table.
 */

#include <glidix/util/common.h>
#include <glidix/fs/vfs.h>
#include <glidix/thread/semaphore.h>

/**
 * Maximum number of files that can be opened per file table. A file table is per-process,
 * and all threads in one process share the table.
 */
#define	MAX_OPEN_FILES					256

typedef struct
{
	int						refcount;			// number of threads using this table.
	struct
	{
		File*					fp;
		int					flags;
	}						list[MAX_OPEN_FILES];
	Semaphore					lock;
} FileTable;

FileTable *ftabCreate();
void ftabUpref(FileTable *ftab);
void ftabDownref(FileTable *ftab);
FileTable *ftabDup(FileTable *ftab);

/**
 * Get the File pointer in the given position and upref it. Returns NULL if the descriptor is invalid.
 */
File* ftabGet(FileTable *ftab, int fd);

/**
 * Allocate a new file descriptor. Returns -1 if no more descriptors are available. The descriptor becomes
 * reserved and you MUST set it with ftabSet() (perhaps to NULL).
 */
int ftabAlloc(FileTable *ftab);

/**
 * Set a file descriptor. It MUST have been just allocated with ftabAlloc(). The reference count of the file
 * will not be changed (so it should be 1 right now if it will be only on this list). The flags are 0 or
 * FD_CLOEXEC for now.
 */
void ftabSet(FileTable *ftab, int fd, File *fp, int flags);

/**
 * Close a file descriptor. Nothing happens if the descriptor is NULL. If the descriptor is reserved and not
 * set, also nothing happens. If the descriptor is set, then vfsClose() is called on the description.
 * 0 is returned on success, errno on error.
 */
int ftabClose(FileTable *ftab, int fd);

/**
 * Try placing a file in a slot. This is used by dup2(). Returns 0 on success, or an error number on error.
 * The error may be EBADF if the descriptor is out of range, or EBUSY if there was a race condtition with
 * a file-opening function such as open() or socket(). 'flags' are the new descriptor flags (may be set
 * by dup3()).
 */
int ftabPut(FileTable *ftab, int fd, File *fp, int flags);

/**
 * Close all files marked with close-on-exec.
 */
void ftabCloseOnExec(FileTable *ftab);

/**
 * Get descriptor flags. Return -1 if EBADF.
 */
int ftabGetFlags(FileTable *ftab, int fd);

/**
 * Set descriptor flags. Return 0 on success, error number on error.
 */
int ftabSetFlags(FileTable *ftab, int fd, int flags);

#endif
