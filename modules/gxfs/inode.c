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

#include <glidix/common.h>
#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/fsdriver.h>
#include <glidix/string.h>
#include <glidix/memory.h>
#include <glidix/vfs.h>
#include <glidix/sched.h>

#include "gxfs.h"

static int tree_load(FileTree *ft, off_t pos, void *buffer)
{
	InodeInfo *info = (InodeInfo*) ft->data;
	gxfsReadInode(info, buffer, PAGE_SIZE, pos);
	return 0;
};

static int tree_flush(FileTree *ft, off_t pos, const void *buffer)
{
	InodeInfo *info = (InodeInfo*) ft->data;
	
	size_t sz = PAGE_SIZE;
	if ((pos + sz) > ft->size)
	{
		sz = ft->size - pos;
	};

	gxfsWriteInode(info, buffer, sz, pos);
	return 0;
};

static void tree_update(FileTree *ft)
{
	InodeInfo *info = (InodeInfo*) ft->data;
	info->data.inoSize = ft->size;
	info->dirty = 1;
};

InodeInfo* gxfsGetInode(GXFS *gxfs, uint64_t ino)
{
	semWait(&gxfs->semInodes);
	
	InodeInfo *info;
	for (info=gxfs->firstIno; info!=NULL; info=info->next)
	{
		if (info->index == ino)
		{
			if (info->refcount == 0)
			{
				__sync_fetch_and_add(&gxfs->numOpenInodes, 1);
			};
			
			__sync_fetch_and_add(&info->refcount, 1);
			semSignal(&gxfs->semInodes);
			return info;
		};
	};
	
	__sync_fetch_and_add(&gxfs->numOpenInodes, 1);
	info = NEW(InodeInfo);
	info->fs = gxfs;
	info->refcount = 1;
	info->index = ino;
	info->dirty = 0;
	info->ft = NULL;
	
	if (vfsPRead(gxfs->fp, &info->data, 512, 0x200000 + (ino << 9)) != 512)
	{
		kfree(info);
		semSignal(&gxfs->semInodes);
		return NULL;
	};

	if ((info->data.inoMode & 0xF000) == 0)
	{
		info->ft = ftCreate(0);
		info->ft->data = info;
		info->ft->load = tree_load;
		info->ft->flush = tree_flush;
		info->ft->update = tree_update;
		info->ft->size = info->data.inoSize;
		ftDown(info->ft);			// make the refcount 0 (otherwise it won't flush)
	};

	semInit(&info->lock);
	
	semInit(&info->semDir);
	info->dirDirty = 0;
	info->dir = NULL;
	
	info->prev = gxfs->lastIno;
	info->next = NULL;
	
	if (gxfs->firstIno == NULL)
	{
		gxfs->firstIno = gxfs->lastIno = info;
	}
	else
	{
		gxfs->lastIno->next = info;
		gxfs->lastIno = info;
	};
	
	semSignal(&gxfs->semInodes);
	return info;
};

void gxfsDeleteTree(GXFS *gxfs, uint16_t depth, uint64_t root)
{
	if (depth != 0)
	{
		GXFS_Indirection ind;
		if (vfsPRead(gxfs->fp, &ind, 512, 0x200000 + (root << 9)) == 512)
		{
			int i;
			for (i=0; i<64; i++)
			{
				if (ind.ptrTable[i] != 0)
				{
					gxfsDeleteTree(gxfs, depth-1, ind.ptrTable[i]);
				};
			};
		};
	};
	
	gxfsFreeBlock(gxfs, root);
};

void gxfsDownrefInode(InodeInfo *info)
{
	if (__sync_add_and_fetch(&info->refcount, -1) == 0)
	{
		GXFS *gxfs = info->fs;
		
		if (info->data.inoLinks == 0)
		{
			if ((info->data.inoMode & 0xF000) != 0x5000)
			{
				// we delete the tree if this is not a symbolic link
				// (symbolic links have no trees attached)
				gxfsDeleteTree(info->fs, info->data.inoTreeDepth, info->data.inoRoot);
			};
			
			gxfsFreeBlock(info->fs, info->index);

			if (info->ft != NULL)
			{
				ftUp(info->ft);
				ftUncache(info->ft);
				ftDown(info->ft);
			};

			GXFS *gxfs = info->fs;
			semWait(&gxfs->semInodes);
			
			if (info->prev == NULL)
			{
				gxfs->firstIno = info->next;
			}
			else
			{
				info->prev->next = info->next;
			};
		
			if (info->next != NULL)
			{
				info->next->prev = info->prev;
			}
			else
			{
				gxfs->lastIno = info->prev;
			};

			semSignal(&gxfs->semInodes);
			kfree(info);
		}
		else
		{
			if (info->dir != NULL)
			{
				if (info->dirDirty)
				{
					gxfsFlushDir(info);
				};
			};

			if (info->dirty)
			{
				vfsPWrite(info->fs->fp, &info->data, 512, 0x200000 + (info->index << 9));
			};
		};
		
		__sync_fetch_and_add(&gxfs->numOpenInodes, -1);
	};
};

void gxfsFlushInode(InodeInfo *info)
{
	if (info->dirty)
	{
		vfsPWrite(info->fs->fp, &info->data, 512, 0x200000 + (info->index << 9));
	};
};

/**
 * Expand an inode's pointer tree by one level. Call this only when the inode is locked.
 * Return 0 on success, -1 on failure (disk full).
 */
static int gxfsExpandTree(InodeInfo *info)
{
	uint64_t newBlock = gxfsAllocBlock(info->fs);
	if (newBlock == 0) return -1;
	
	GXFS_Indirection ind;
	memset(&ind, 0, 512);
	ind.ptrTable[0] = info->data.inoRoot;
	
	vfsPWrite(info->fs->fp, &ind, 512, 0x200000 + (newBlock << 9));
	info->data.inoRoot = newBlock;
	info->data.inoTreeDepth++;
	info->dirty = 1;
	
	return 0;
};

/**
 * Find the block which contains the (512-byte-aligned) offset in the given inode. If 'make'
 * is 1, then the block will be allocated if not already done. Returns the block index, or
 * 0 if the block wasn't found or we ran out of space trying to allocate it.
 * Call this only when the inode is locked!
 */
static uint64_t gxfsGetBlockFromOffset(InodeInfo *info, uint64_t offset, int make)
{
	if ((offset & 0x8000000000000000UL) != 0)
	{
		// top bit must be clear
		return 0;
	};
	
	uint16_t depthNeeded;
	for (depthNeeded=0;;depthNeeded++)
	{
		if (offset < (1UL << (9 + 6 * depthNeeded))) break;
	};
	
	if ((depthNeeded > info->data.inoTreeDepth) && (!make))
	{
		return 0;
	};
	
	while (info->data.inoTreeDepth < depthNeeded)
	{
		gxfsExpandTree(info);
	};
	
	uint64_t block = info->data.inoRoot;
	uint16_t i;
	for (i=0; i<info->data.inoTreeDepth; i++)
	{
		uint64_t bitpos = 9 + 6 * (info->data.inoTreeDepth - i - 1);
		uint64_t index = (offset >> bitpos) & 0x3F;
		
		GXFS_Indirection ind;
		if (vfsPRead(info->fs->fp, &ind, 512, 0x200000 + (block << 9)) != 512)
		{
			return 0;
		};
		
		if (ind.ptrTable[index] == 0)
		{
			if (make)
			{
				uint64_t newBlock = gxfsAllocBlock(info->fs);
				if (newBlock == 0)
				{
					return 0;
				};
				
				gxfsZeroBlock(info->fs, newBlock);
				ind.ptrTable[index] = newBlock;
				vfsPWrite(info->fs->fp, &ind, 512, 0x200000 + (block << 9));
			}
			else
			{
				return 0;
			};
		};
		
		block = ind.ptrTable[index];
	};
	
	return block;
};

ssize_t gxfsReadInode(InodeInfo *info, void *buffer, size_t size, off_t pos)
{
	semWait(&info->lock);
	ssize_t sizeRead = 0;
	
	if (pos >= info->data.inoSize)
	{
		semSignal(&info->lock);
		return 0;
	};
	
	if ((pos+size) > info->data.inoSize)
	{
		size = info->data.inoSize - pos;
	};
	
	uint8_t *put = (uint8_t*) buffer;
	while (size > 0)
	{
		uint64_t relblock = (uint64_t) pos & ~0x1FFUL;
		uint64_t offset = (uint64_t) pos & 0x1FFUL;
		
		uint64_t block = gxfsGetBlockFromOffset(info, relblock, 1);
		if (block == 0) break;
		
		size_t readNow = 512 - offset;
		if (readNow > size)
		{
			readNow = size;
		};
		
		ssize_t okNow = vfsPRead(info->fs->fp, put, readNow, 0x200000 + (block << 9) + offset);
		if (okNow == -1)
		{
			semSignal(&info->lock);
			if (sizeRead != 0) return sizeRead;
			ERRNO = EIO;
			return -1;
		};
		
		size -= okNow;
		put += okNow;
		pos += okNow;
		sizeRead += okNow;
	};
	
	info->data.inoAccessTime = time();
	info->dirty = 1;
	semSignal(&info->lock);
	return sizeRead;
};

void gxfsDeleteBranches(GXFS *gxfs, uint16_t depth, uint64_t block, uint64_t offset, uint64_t newSize)
{
	if (depth == 0) return;
	
	uint64_t indEntryScope = (1 << (9 + 6 * (depth-1)));
	
	GXFS_Indirection ind;
	if (vfsPRead(gxfs->fp, &ind, 512, 0x200000 + (block << 9)) == 512)
	{
		int i;
		for (i=0; i<64; i++)
		{
			if (ind.ptrTable[i] != 0)
			{
				uint64_t beginsAt = offset + indEntryScope * i;
				uint64_t endsAt = beginsAt + indEntryScope;
				
				if (beginsAt >= newSize)
				{
					gxfsDeleteTree(gxfs, depth-1, ind.ptrTable[i]);
					ind.ptrTable[i] = 0;
				}
				else if (endsAt >= newSize)
				{
					// PARTS of this to be deleted
					gxfsDeleteBranches(gxfs, depth-1, ind.ptrTable[i], beginsAt, newSize);
				};
			};
		};
		
		vfsPWrite(gxfs->fp, &ind, 512, 0x200000 + (block << 9));
	};
};

static int gxfsResizeUnlocked(InodeInfo *info, size_t newSize)
{
	if (newSize < info->data.inoSize)
	{
		// walk down the tree and free any blocks which are above the new size
		gxfsDeleteBranches(info->fs, info->data.inoTreeDepth, info->data.inoRoot, 0, newSize);
	};

	if (newSize & 0x1FF)
	{
		// zero out the final part of the last block
		uint64_t block = gxfsGetBlockFromOffset(info, (newSize & ~0x1FFUL), 0);
		if (block != 0)
		{
			uint64_t offset = newSize & 0x1FFUL;
			uint64_t size = 512 - offset;
			
			char zeroes[512];
			memset(zeroes, 0, 512);
			
			vfsPWrite(info->fs->fp, zeroes, size, 0x200000 + (block << 9) + offset);
		};
	};

	info->data.inoSize = newSize;
	info->dirty = 1;
	return 0;
};

int gxfsResize(InodeInfo *info, size_t newSize)
{
	if ((newSize & 0x8000000000000000UL) != 0)
	{
		return -1;
	};
	
	semWait(&info->lock);
	int result = gxfsResizeUnlocked(info, newSize);
	semSignal(&info->lock);
	
	return result;
};

ssize_t gxfsWriteInode(InodeInfo *info, const void *buffer, size_t size, off_t pos)
{
	semWait(&info->lock);
	ssize_t sizeWritten = 0;
	
	if ((pos+size) > info->data.inoSize)
	{
		if (gxfsResizeUnlocked(info, pos+size) != 0)
		{
			semSignal(&info->lock);
			ERRNO = EIO;
			return -1;
		};
	};
	
	const uint8_t *scan = (const uint8_t*) buffer;
	while (size > 0)
	{
		uint64_t relblock = (uint64_t) pos & ~0x1FFUL;
		uint64_t offset = (uint64_t) pos & 0x1FFUL;
		
		uint64_t block = gxfsGetBlockFromOffset(info, relblock, 1);
		if (block == 0)
		{
			semSignal(&info->lock);
			if (sizeWritten != 0) return sizeWritten;
			ERRNO = ENOSPC;
			return -1;
		};
		
		size_t writeNow = 512 - offset;
		if (writeNow > size)
		{
			writeNow = size;
		};
		
		ssize_t okNow = vfsPWrite(info->fs->fp, scan, writeNow, 0x200000 + (block << 9) + offset);
		if (okNow == -1)
		{
			semSignal(&info->lock);
			if (sizeWritten != 0) return sizeWritten;
			return -1;
		};
		
		size -= okNow;
		scan += okNow;
		pos += okNow;
		sizeWritten += okNow;
	};
	
	info->data.inoAccessTime = info->data.inoModTime = time();
	info->dirty = 1;
	semSignal(&info->lock);
	return sizeWritten;
};
