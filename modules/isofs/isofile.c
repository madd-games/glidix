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
#include <glidix/vfs.h>
#include <glidix/memory.h>
#include <glidix/semaphore.h>
#include <glidix/string.h>
#include <glidix/console.h>

#include "isofs.h"
#include "isofile.h"

typedef struct
{
	struct stat			st;
	ISOFileSystem*			isofs;
	uint64_t			start;
	uint64_t			size;
	off_t				pos;	
} ISOFile;

static void isofile_close(File *fp)
{
	ISOFile *file = (ISOFile*) fp->fsdata;
	semWait(&file->isofs->sem);
	file->isofs->numOpenInodes--;
	semSignal(&file->isofs->sem);
	kfree(fp->fsdata);
};

static int isofile_dup(File *me, File *fp, size_t szfile)
{
	ISOFile *org = (ISOFile*) me->fsdata;
	semWait(&org->isofs->sem);
	org->isofs->numOpenInodes++;
	ISOFile *file = (ISOFile*) kmalloc(sizeof(ISOFile));
	memcpy(file, me->fsdata, sizeof(ISOFile));
	memcpy(fp, me, szfile);
	fp->fsdata = file;
	semSignal(&org->isofs->sem);
	return 0;
};

static off_t isofile_seek(File *fp, off_t offset, int whence)
{
	ISOFile *file = (ISOFile*) fp->fsdata;

	off_t newOffset;
	switch (whence)
	{
	case SEEK_SET:
		newOffset = offset;
		break;
	case SEEK_CUR:
		newOffset = file->pos + offset;
		break;
	case SEEK_END:
		newOffset = (off_t)file->size + offset;
		break;
	};

	if (newOffset > (off_t)file->size)
	{
		newOffset = (off_t)file->size;
	};

	if (newOffset < 0)
	{
		newOffset = 0;
	};

	file->pos = newOffset;
	return newOffset;
};

static ssize_t isofile_read(File *fp, void *buffer, size_t size)
{
	ISOFile *file = (ISOFile*) fp->fsdata;
	semWait(&file->isofs->sem);
	File *fsimg = file->isofs->fp;
	fsimg->seek(fsimg, file->start + file->pos, SEEK_SET);

	if ((file->pos+size) > file->size)
	{
		size = file->size - file->pos;
	};

	ssize_t count = vfsRead(fsimg, buffer, size);
	semSignal(&file->isofs->sem);
	if (count == -1)
	{
		return -1;
	};

	file->pos += count;
	return count;
};

static ssize_t isofile_pread(File *fp, void *buffer, size_t size, off_t off)
{
	ISOFile *file = (ISOFile*) fp->fsdata;
	semWait(&file->isofs->sem);
	File *fsimg = file->isofs->fp;
	fsimg->seek(fsimg, file->start + off, SEEK_SET);

	if (off > file->size)
	{
		semSignal(&file->isofs->sem);
		return 0;
	};
	
	if ((off+size) > file->size)
	{
		size = file->size - off;
	};

	ssize_t count = vfsRead(fsimg, buffer, size);
	semSignal(&file->isofs->sem);
	if (count == -1)
	{
		return -1;
	};

	return count;
};

static int isofile_fstat(File *fp, struct stat *st)
{
	ISOFile *file = (ISOFile*) fp->fsdata;
	memcpy(st, &file->st, sizeof(struct stat));
	return 0;
};

static void isofile_pollinfo(File *fp, Semaphore **sems)
{
	sems[PEI_READ] = vfsGetConstSem();
	sems[PEI_WRITE] = vfsGetConstSem();
};

int isoOpenFile(ISOFileSystem *isofs, uint64_t start, uint64_t size, File *fp, struct stat *st)
{
	ISOFile *file = (ISOFile*) kmalloc(sizeof(ISOFile));
	semWait(&isofs->sem);
	memcpy(&file->st, st, sizeof(struct stat));
	file->isofs = isofs;
	file->start = start;
	file->size = size;
	file->pos = 0;

	fp->fsdata = file;
	fp->close = isofile_close;
	fp->read = isofile_read;
	fp->pread = isofile_pread;
	fp->dup = isofile_dup;
	fp->seek = isofile_seek;
	fp->fstat = isofile_fstat;
	fp->pollinfo = isofile_pollinfo;

	isofs->numOpenInodes++;
	semSignal(&isofs->sem);
	return 0;
};
