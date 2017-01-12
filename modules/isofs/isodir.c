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
#include <glidix/semaphore.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/vfs.h>
#include <glidix/console.h>
#include <glidix/time.h>
#include <glidix/sched.h>

#include "isodir.h"
#include "isofile.h"

typedef struct
{
	ISOFileSystem *isofs;
	uint64_t pos;
	uint64_t nextPos;
	uint64_t end;

	uint64_t childStart;
	uint64_t childEnd;
} ISODirScanner;

static void isodir_close(Dir *dir)
{
	ISODirScanner *isodir = (ISODirScanner*) dir->fsdata;
	semWait(&isodir->isofs->sem);
	isodir->isofs->numOpenInodes--;
	semSignal(&isodir->isofs->sem);
	kfree(dir->fsdata);
};

static void translateName(char *dst, const char *src)
{
	while (1)
	{
		char c = *src++;
		if (c == 0) break;
		if (c == ';') break;

		if ((c >= 'A') && (c <= 'Z')) c = c-'A'+'a';
		*dst++ = c;
	};
	if (*(dst-1) == '.') dst--;
	*dst = 0;
};

static int readDirent(ISODirScanner *isodir, Dir *dir)
{
	semWait(&isodir->isofs->sem);
	File *fp = isodir->isofs->fp;
	fp->seek(fp, isodir->pos, SEEK_SET);

	ISODirentHeader head;
	vfsRead(fp, &head, sizeof(ISODirentHeader));
	isodir->nextPos = isodir->pos + head.size;

	while ((head.size == 0) && (isodir->pos < isodir->end))
	{
		isodir->pos++;
		if (isodir->pos == isodir->end)
		{
			semSignal(&isodir->isofs->sem);
			return -1;
		};
		
		fp->seek(fp, isodir->pos, SEEK_SET);
		vfsRead(fp, &head, sizeof(ISODirentHeader));
		isodir->nextPos = isodir->pos + head.size;
	};

	dir->dirent.d_ino = isodir->pos;
	char namebuf[256];
	vfsRead(fp, namebuf, head.filenameLen);
	namebuf[head.filenameLen] = 0;
	translateName(dir->dirent.d_name, namebuf);

	dir->stat.st_dev = 0;
	dir->stat.st_ino = isodir->pos;
	dir->stat.st_mode = 0555;
	if (head.flags & 2) dir->stat.st_mode |= VFS_MODE_DIRECTORY;
	dir->stat.st_nlink = 1;
	dir->stat.st_uid = 0;
	dir->stat.st_gid = 0;
	dir->stat.st_rdev = 0;
	dir->stat.st_size = head.fileSize;
	dir->stat.st_blksize = isodir->isofs->blockSize;
	dir->stat.st_blocks = dir->stat.st_size / dir->stat.st_blksize + 1;

	time_t time = makeUnixTime((int64_t)head.year + 1900, head.month, head.day, head.hour, head.minute, head.second);
	dir->stat.st_ctime = time;
	dir->stat.st_atime = time;
	dir->stat.st_mtime = time;

	dir->stat.st_ixperm = XP_ALL;
	dir->stat.st_oxperm = 0;
	dir->stat.st_dxperm = XP_ALL;
	
	isodir->childStart = (uint64_t)head.startLBA * isodir->isofs->blockSize;
	isodir->childEnd = isodir->childStart + (uint64_t)head.fileSize;

	semSignal(&isodir->isofs->sem);
	return 0;
};

static int isodir_opendir(Dir *me, Dir *dir, size_t szDir)
{
	ISODirScanner *isodir = (ISODirScanner*) me->fsdata;
	return isodirOpen(isodir->isofs, isodir->childStart, isodir->childEnd, dir, szDir);
};

static int isodir_openfile(Dir *me, File *fp, size_t szFile)
{
	ISODirScanner *isodir = (ISODirScanner*) me->fsdata;
	if (me->stat.st_mode & VFS_MODE_DIRECTORY) return VFS_BUSY;
	return isoOpenFile(isodir->isofs, isodir->childStart, isodir->childEnd-isodir->childStart, fp, &me->stat);
};

static int isodir_next(Dir *dir)
{
	ISODirScanner *isodir = (ISODirScanner*) dir->fsdata;
	if (isodir->nextPos == isodir->end)
	{
		return -1;
	};

	isodir->pos = isodir->nextPos;
	return readDirent(isodir, dir);
};

int isodirOpen(ISOFileSystem *isofs, uint64_t start, uint64_t end, Dir *dir, size_t szdir)
{
	ISODirScanner *isodir = (ISODirScanner*) kmalloc(sizeof(ISODirScanner));
	isodir->isofs = isofs;
	isodir->pos = start;
	isodir->end = end;
	semWait(&isodir->isofs->sem);
	isodir->isofs->numOpenInodes++;
	semSignal(&isodir->isofs->sem);

	dir->fsdata = isodir;
	dir->close = isodir_close;
	dir->next = isodir_next;
	dir->opendir = isodir_opendir;
	dir->openfile = isodir_openfile;

	readDirent(isodir, dir);

	// the first two records are "." and "..", we don't report them to glidix
	isodir_next(dir);
	int status = isodir_next(dir);
	if (status != 0)
	{
		return VFS_EMPTY_DIRECTORY;
	};

	return 0;
};
