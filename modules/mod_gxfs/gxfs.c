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

#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/fsdriver.h>
#include <glidix/string.h>
#include <glidix/memory.h>

#include "gxfs.h"

static Spinlock gxfsMountLock;
static volatile int numMountedFilesystems = 0;

static int gxfs_openroot(FileSystem *fs, Dir *dir, size_t szdir)
{
	GXFileSystem *gxfs = (GXFileSystem*) fs->fsdata;
	//kprintf_debug("gxfs_openroot\n");
	return GXOpenDir(gxfs, dir, 2);				// inode 2 = root directory
};

static int gxfs_unmount(FileSystem *fs)
{
	GXFileSystem *gxfs = (GXFileSystem*) fs->fsdata;
	semWait(&gxfs->sem);
	if (gxfs->numOpenInodes != 0)
	{
		kprintf_debug("gxfs: cannot unmount: %d inodes are open\n", gxfs->numOpenInodes);
		semSignal(&gxfs->sem);
		return -1;
	};

	kfree(gxfs->sections);
	vfsClose(gxfs->fp);
	kfree(gxfs);

	spinlockAcquire(&gxfsMountLock);
	numMountedFilesystems--;
	spinlockRelease(&gxfsMountLock);
	return 0;
};

static void gxfs_getinfo(FileSystem *fs, FSInfo *info)
{
	GXFileSystem *gxfs = (GXFileSystem*) fs->fsdata;
	semWait(&gxfs->sem);
	
	info->fs_usedino = gxfs->usedInodes;
	info->fs_usedblk = gxfs->usedBlocks;
	info->fs_inodes = gxfs->cis.cisTotalIno;
	info->fs_blocks = gxfs->cis.cisTotalBlocks;
	info->fs_blksize = gxfs->cis.cisBlockSize;
	
	semSignal(&gxfs->sem);
};

static uint64_t countBitmapUse(File *fp, uint64_t max)
{
	uint64_t result = 0;
	max = (max + 7UL) & ~7UL;
	max /= 8;
	
	while (max > 0)
	{
		uint64_t buffer = 0;
		size_t toRead = 8;
		if (toRead > max) toRead = max;
		
		vfsRead(fp, &buffer, toRead);
		max -= toRead;

		uint64_t count = 64;
		while (count--)
		{
			result += (buffer & 1);
			buffer >>= 1;
		};
	};
	
	return result;
};

static int gxfsMount(const char *image, FileSystem *fs, size_t szfs)
{
	spinlockAcquire(&gxfsMountLock);

	int error;
	File *fp = vfsOpen(image, 0, &error);
	if (fp == NULL)
	{
		kprintf("gxfs: could not open %s\n", image);
		spinlockRelease(&gxfsMountLock);
		return -1;
	};

	if (fp->seek == NULL)
	{
		vfsClose(fp);
		spinlockRelease(&gxfsMountLock);
		kprintf("gxfs: this file does not support seeking\n");
		return -1;
	};

	fp->seek(fp, 0x10, SEEK_SET);
	uint64_t offCIS;
	if (vfsRead(fp, &offCIS, 8) != 8)
	{
		kprintf("gxfs: offCIS cannot be read, this is not a valid GXFS image\n");
		vfsClose(fp);
		spinlockRelease(&gxfsMountLock);
		return -1;
	};

	gxfsCIS cis;
	fp->seek(fp, offCIS, SEEK_SET);
	if (vfsRead(fp, &cis, 64) != 64)
	{
		kprintf("gxfs: cannot read the whole CIS, this is not a valid GXFS image\n");
		vfsClose(fp);
		spinlockRelease(&gxfsMountLock);
		return -1;
	};

	if (memcmp(cis.cisMagic, "GXFS", 4) != 0)
	{
		kprintf("gxfs: invalid CIS magic, this is not a valid GXFS image\n");
		vfsClose(fp);
		spinlockRelease(&gxfsMountLock);
		return -1;
	};

	kprintf_debug("gxfs: filesystem creation time: %ld\n", cis.cisCreateTime);

	size_t numSections = cis.cisTotalIno / cis.cisInoPerSection;
	if (numSections != (cis.cisTotalBlocks / cis.cisBlocksPerSection))
	{
		kprintf("gxfs: section count inconsistent\n");
		vfsClose(fp);
		spinlockRelease(&gxfsMountLock);
		return -1;
	};

	kprintf_debug("gxfs: this filesystem has %lu sections\n", numSections);

	GXFileSystem *gxfs = (GXFileSystem*) kmalloc(sizeof(GXFileSystem) + cis.cisBlockSize);
	memset(gxfs, 0, sizeof(GXFileSystem) + cis.cisBlockSize);
	gxfs->fp = fp;
	semInit(&gxfs->sem);
	memcpy(&gxfs->cis, &cis, sizeof(gxfsCIS));
	gxfs->numSections = numSections;
	gxfs->sections = (gxfsSD*) kmalloc(sizeof(gxfsSD)*numSections);
	fp->seek(fp, cis.cisOffSections, SEEK_SET);
	vfsRead(fp, gxfs->sections, 32*numSections);
	gxfs->numOpenInodes = 0;
	memset(gxfs->ibuf, 0, sizeof(BufferedInode)*INODE_BUFFER_SIZE);

	fs->fsdata = gxfs;
	fs->fsname = "gxfs";
	fs->openroot = gxfs_openroot;
	fs->unmount = gxfs_unmount;
	fs->getinfo = gxfs_getinfo;
	
	uint64_t i;
	for (i=0; i<numSections; i++)
	{
		fp->seek(fp, gxfs->sections[i].sdOffMapIno, SEEK_SET);
		gxfs->usedInodes += countBitmapUse(fp, cis.cisInoPerSection);
		
		fp->seek(fp, gxfs->sections[i].sdOffMapBlocks, SEEK_SET);
		gxfs->usedBlocks += countBitmapUse(fp, cis.cisBlocksPerSection);
	};
	
	numMountedFilesystems++;
	spinlockRelease(&gxfsMountLock);
	return 0;
};

FSDriver gxfsDriver = {
	.onMount = gxfsMount,
	.name = "gxfs"
};

MODULE_INIT()
{
	kprintf("gxfs: registering the GXFS filesystem\n");
	spinlockRelease(&gxfsMountLock);
	registerFSDriver(&gxfsDriver);
	return 0;
};

MODULE_FINI()
{
	spinlockAcquire(&gxfsMountLock);
	int numFS = numMountedFilesystems;
	spinlockRelease(&gxfsMountLock);

	if (numFS != 0)
	{
		return 1;
	};

	return 0;
};
