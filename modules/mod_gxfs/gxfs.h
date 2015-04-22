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

#ifndef GXFS_H
#define GXFS_H

#include <glidix/common.h>
#include <glidix/vfs.h>
#include <glidix/semaphore.h>

#define	GX_NO_BLOCK			0xFFFFFFFFFFFFFFFF

typedef struct
{
	char				cisMagic[4];
	uint64_t			cisTotalIno;
	uint64_t			cisInoPerSection;
	uint64_t			cisTotalBlocks;
	uint64_t			cisBlocksPerSection;
	uint16_t			cisBlockSize;
	int64_t				cisCreateTime;
	uint64_t			cisFirstDataIno;
	uint64_t			cisOffSections;
	uint16_t			cisZero;
} __attribute__ ((packed)) gxfsCIS;

typedef struct
{
	uint64_t			sdOffMapIno;
	uint64_t			sdOffMapBlocks;
	uint64_t			sdOffTabIno;
	uint64_t			sdOffTabBlocks;
} __attribute__ ((packed)) gxfsSD;

typedef struct
{
	uint64_t			fOff;
	uint64_t			fBlock;
	uint32_t			fExtent;
} __attribute__ ((packed)) gxfsFragment;

typedef struct
{
	uint16_t			inoMode;
	uint64_t			inoSize;
	uint8_t				inoLinks;
	int64_t				inoCTime;
	int64_t				inoATime;
	int64_t				inoMTime;
	uint16_t			inoOwner;
	uint16_t			inoGroup;
	gxfsFragment			inoFrags[16];
} __attribute__ ((packed)) gxfsInode;

/**
 * The 'fsdata' of a mounted GXFS filesystem.
 */
typedef struct
{
	// the device/image handle
	File*				fp;

	// the semaphore for this mountpoint.
	Semaphore			sem;

	// the CIS.
	gxfsCIS				cis;

	// sections
	size_t				numSections;
	gxfsSD*				sections;

	// number of inodes that are open
	int				numOpenInodes;
} GXFileSystem;

/**
 * Describes an open inode.
 */
typedef struct
{
	// the filesystem
	GXFileSystem*			gxfs;

	// on-disk offset to inode
	uint64_t			offset;

	// the inode number
	ino_t				ino;

	// current file position
	off_t				pos;
} GXInode;

/**
 * Describes an open directory pointer.
 */
typedef struct
{
	// the filesystem
	GXFileSystem*			gxfs;

	// the inode
	GXInode				gxino;
} GXDir;

/**
 * Open file description.
 */
typedef struct
{
	GXFileSystem*			gxfs;
	GXInode				gxino;
	int				dirty;
} GXFile;

ino_t GXCreateInode(GXFileSystem *gxfs, GXInode *gxino, ino_t closeTo);
int GXOpenInode(GXFileSystem *gxfs, GXInode *gxino, ino_t inode);
size_t GXReadInode(GXInode *gxino, void *buffer, size_t size);
size_t GXWriteInode(GXInode *gxino, const void *buffer, size_t size);
void GXReadInodeHeader(GXInode *gxino, gxfsInode *inode);			// inoFrags is NOT read!
void GXWriteInodeHeader(GXInode *gxino, gxfsInode *inode);			// inoFrags is NOT updated!
void GXShrinkInode(GXInode *gxino, size_t shrinkBy, gxfsInode *inode);
void GXUnlinkInode(GXInode *gxino);
void GXDumpInode(GXFileSystem *gxfs, ino_t ino);

uint64_t GXAllocBlock(GXFileSystem *gxfs, uint64_t tryFirst);
void GXFreeBlock(GXFileSystem *gxfs, uint64_t block);

int GXOpenDir(GXFileSystem *gxfs, Dir *dir, ino_t ino);
int GXOpenFile(GXFileSystem *gxfs, File *fp, ino_t ino);

#endif
