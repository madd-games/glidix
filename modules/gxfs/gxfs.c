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

#include "gxfs.h"

static int numMounts = 0;

static uint64_t gxfsChecksum(const void *block)
{
	const uint64_t *scan = (const uint64_t*) block;
	uint64_t sum = 0;
	
	int i;
	for (i=0; i<6; i++)
	{
		sum += scan[i];
	};
	
	return sum;
};

static void gxfs_getinfo(FileSystem *fs, FSInfo *info)
{
	GXFS *gxfs = (GXFS*) fs->fsdata;
	info->fs_usedblk = gxfs->sb.sbUsedBlocks;
	info->fs_blocks = gxfs->sb.sbTotalBlocks;
	info->fs_blksize = 512;
	memcpy(info->fs_bootid, gxfs->sb.sbMGSID, 16);
};

static int gxfs_unmount(FileSystem *fs)
{
	GXFS *gxfs = (GXFS*) fs->fsdata;
	
	mutexLock(&gxfs->mtxInodes);
	if (gxfs->numOpenInodes != 0)
	{
		kprintf("gxfs: cannot unmount because some inodes are open (%d)\n", gxfs->numOpenInodes);
		mutexUnlock(&gxfs->mtxInodes);
		return -1;
	};
	
	while (gxfs->firstIno != NULL)
	{
		InodeInfo *info = gxfs->firstIno;
		gxfs->firstIno = info->next;
		
		if (info->ft != NULL)
		{
			ftUp(info->ft);
			ftUncache(info->ft);
			ftDown(info->ft);
		};
		
		while (info->dir != NULL)
		{
			DirentInfo *dirent = info->dir;
			info->dir = dirent->next;
			kfree(dirent);
		};
		
		kfree(info);
	};
	
	vfsClose(gxfs->fp);
	kfree(gxfs);
	__sync_fetch_and_add(&numMounts, -1);
	return 0;
};

static int gxfs_openroot(FileSystem *fs, Dir *dir, size_t szdir)
{
	return gxfsOpenDir((GXFS*)fs->fsdata, 2, dir, szdir);
};

static int gxfsMount(const char *image, FileSystem *fs, size_t szfs)
{
	int error;
	File *fp = vfsOpen(image, 0, &error);
	if (fp == NULL)
	{
		kprintf("gxfs: failed to open %s\n", image);
		return -1;
	};
	
	GXFS_Superblock sb;
	if (vfsPRead(fp, &sb, 512, 0x200000) != 512)
	{
		kprintf("gxfs: failed to read the superblock\n");
		vfsClose(fp);
		return -1;
	};
	
	if (gxfsChecksum(&sb) != 0)
	{
		kprintf("gxfs: superblock checksum invalid\n");
		vfsClose(fp);
		return -1;
	};
	
	if (sb.sbMagic != GXFS_MAGIC)
	{
		kprintf("gxfs: superblock magic number invalid\n");
		vfsClose(fp);
		return -1;
	};
	
	__sync_fetch_and_add(&numMounts, 1);
	
	GXFS *gxfs = NEW(GXFS);
	gxfs->fp = fp;
	memcpy(&gxfs->sb, &sb, 512);
	
	mutexInit(&gxfs->mtxInodes);
	gxfs->firstIno = NULL;
	gxfs->lastIno = NULL;
	gxfs->numOpenInodes = 0;
	
	mutexInit(&gxfs->mtxAlloc);
	
	fs->fsdata = gxfs;
	fs->fsname = "gxfs";
	fs->getinfo = gxfs_getinfo;
	fs->unmount = gxfs_unmount;
	fs->openroot = gxfs_openroot;
	
	return 0;
};

FSDriver gxfsDriver = {
	.onMount = gxfsMount,
	.name = "gxfs"
};

MODULE_INIT()
{
	kprintf("gxfs: registering the GXFS filesystem\n");
	registerFSDriver(&gxfsDriver);
	return MODINIT_OK;
};

MODULE_FINI()
{
	if (numMounts != 0)
	{
		return 1;
	};
	
	return 0;
};
