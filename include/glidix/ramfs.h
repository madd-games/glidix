/*
	Glidix kernel

	Copyright (c) 2014-2016, Madd Games.
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

#ifndef __glidix_ramfs_h
#define __glidix_ramfs_h

#include <glidix/common.h>
#include <glidix/semaphore.h>
#include <glidix/vfs.h>

/**
 * RAM filesystem node.
 */
typedef struct RamfsInode_
{
	/**
	 * Node metadata. The st_ino field is actually a pointer to this node structure;
	 * and st_nlink is in fact the reference count.
	 */
	struct stat				meta;
	
	/**
	 * For directories, this points to the first entry (which is just the directory end pointer
	 * for empty directories). For files, this points to the first byte of the data. For symlinks,
	 * this points to a NUL-terminated string specifying the pathname.
	 */
	void*					data;
} RamfsInode;

/**
 * RAM filesystem directory entry. Those form linked lists, which are ALWAYS terminated by a directory
 * end pointer entry (where d_ino == 0). Empty directories are made from just the end pointer entry.
 * Each directory also starts with a null inode, which is completly ignored, but must have a valid "next"
 * pointer.
 */
typedef struct RamfsDirent_
{
	/**
	 * The entry. If d_ino == 0, then this is a directory end pointer. If d_ino == 1, then this is
	 * an unlinked entry.
	 */
	struct dirent				dent;
	
	/**
	 * Links.
	 */
	struct RamfsDirent_*			prev;
	struct RamfsDirent_*			next;
	struct RamfsDirent_*			up;
} RamfsDirent;

/**
 * RAM filesystem description.
 */
typedef struct Ramfs_
{
	/**
	 * The name of this filesystem. Always starts with a dot (".").
	 */
	char					name[64];
	
	/**
	 * Prvious and next RAM filesystems.
	 */
	struct Ramfs_*				prev;
	struct Ramfs_*				next;
	
	/**
	 * Root inode.
	 */
	RamfsInode				root;
	
	/**
	 * Semaphore controlling access to this filesystem.
	 */
	Semaphore				lock;
} Ramfs;

/**
 * Initialize the RAM filesystem subsystem.
 */
void ramfsInit();

#endif
