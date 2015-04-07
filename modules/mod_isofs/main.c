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

#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/fsdriver.h>
#include <glidix/string.h>
#include <glidix/memory.h>

#include "isofs.h"
#include "isodir.h"

static int iso_openroot(FileSystem *fs, Dir *dir, size_t szdir)
{
	ISOFileSystem *isofs = (ISOFileSystem*) fs->fsdata;
	return isodirOpen(isofs, isofs->rootStart, isofs->rootEnd, dir, szdir);
};

static int checkPVD(ISOPrimaryVolumeDescriptor *pvd)
{
	if (pvd->type != 1)
	{
		return 0;
	};

	if (memcmp(pvd->magic, "CD001", 5) != 0)
	{
		return 0;
	};

	if (pvd->version != 1)
	{
		return 0;
	};

	return 1;
};

static int isoMount(const char *image, FileSystem *fs, size_t szfs)
{
	int error;
	File *fp = vfsOpen(image, 0, &error);
	if (fp == NULL)
	{
		kprintf_debug("isofs: could not open %s\n", image);
		return -1;
	};

	if (fp->seek == NULL)
	{
		kprintf_debug("isofs: %s cannot seek\n", image);
		vfsClose(fp);
		return -1;
	};

	fp->seek(fp, 0x8000, SEEK_SET);
	ISOPrimaryVolumeDescriptor primary;
	if (vfsRead(fp, &primary, sizeof(ISOPrimaryVolumeDescriptor)) != sizeof(ISOPrimaryVolumeDescriptor))
	{
		kprintf_debug("isofs: cannot read the whole ISOPrimaryVolumeDescriptor (EOF)\n");
		vfsClose(fp);
		return -1;
	};

	if (!checkPVD(&primary))
	{
		kprintf_debug("isofs: the Primary Volume Descriptor is invalid\n");
		vfsClose(fp);
		return -1;
	};

	kprintf_debug("isofs: PVD validated\n");

	ISOFileSystem *isofs = (ISOFileSystem*) kmalloc(sizeof(ISOFileSystem));
	isofs->fp = fp;
	semInit(&isofs->sem);

	ISODirentHeader *rootDir = (ISODirentHeader*) &primary.rootDir;
	isofs->rootStart = (uint64_t)rootDir->startLBA * (uint64_t)primary.blockSize;
	isofs->rootEnd = isofs->rootStart + (uint64_t)rootDir->fileSize;
	isofs->blockSize = (uint64_t)primary.blockSize;

	kprintf_debug("isofs: root directory start LBA is %a, block size is %d\n", rootDir->startLBA, primary.blockSize);

	fs->fsdata = isofs;
	fs->fsname = "isofs";
	fs->openroot = iso_openroot;
	return 0;
};

FSDriver isoDriver = {
	.onMount = isoMount,
	.name = "isofs"
};

MODULE_INIT()
{
	kprintf("isofs: registering the ISO filesystem\n");
	registerFSDriver(&isoDriver);
	return 0;
};

MODULE_FINI()
{
	return 0;
};
