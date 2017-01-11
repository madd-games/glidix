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
#include <glidix/time.h>

#include "gxfs.h"

#define	PREFETCH_SIZE				1024

typedef struct
{
	InodeInfo *info;
	off_t pos;
	
	/**
	 * Currently-prefetched block offset, the current size of the buffer
	 * and whether it is dirty.
	 */
	Semaphore lock;
	uint64_t prefetchOff;
	uint8_t prefetchBuf[PREFETCH_SIZE];
	uint64_t prefetchSize;
	int prefetchDirty;
} FileData;

static void gxfs_close(File *fp)
{
	FileData *data = (FileData*) fp->fsdata;
	if (data->prefetchDirty)
	{
		gxfsWriteInode(data->info, data->prefetchBuf, data->prefetchSize, data->prefetchOff);
	};
	__sync_fetch_and_add(&data->info->data.inoLinks, -1);
	data->info->dirty = 1;
	gxfsDownrefInode(data->info);
	kfree(data);
};

static int gxfs_dup(File *me, File *fp, size_t filesz)
{
	FileData *data = (FileData*) me->fsdata;
	memcpy(fp, me, filesz);
	
	FileData *newData = NEW(FileData);
	__sync_fetch_and_add(&data->info->refcount, 1);
	
	memcpy(newData, data, sizeof(FileData));
	fp->fsdata = newData;
	
	return 0;
};

static int gxfs_fstat(File *fp, struct stat *st)
{
	FileData *data = (FileData*) fp->fsdata;
	
	semWait(&data->info->lock);
	st->st_ino = data->info->index;
	st->st_mode = data->info->data.inoMode;
	st->st_nlink = data->info->data.inoLinks;
	st->st_uid = data->info->data.inoOwner;
	st->st_gid = data->info->data.inoGroup;
	st->st_size = data->info->data.inoSize;
	st->st_blksize = 512;
	st->st_blocks = (1 << (6 * data->info->data.inoTreeDepth));
	st->st_atime = data->info->data.inoAccessTime;
	st->st_ctime = data->info->data.inoChangeTime;
	st->st_mtime = data->info->data.inoModTime;
	st->st_ixperm = data->info->data.inoIXPerm;
	st->st_oxperm = data->info->data.inoOXPerm;
	st->st_dxperm = data->info->data.inoDXPerm;
	st->st_btime = data->info->data.inoBirthTime;
	semSignal(&data->info->lock);
	
	return 0;
};

static int gxfs_fchmod(File *fp, mode_t mode)
{
	FileData *data = (FileData*) fp->fsdata;
	data->info->data.inoMode = (data->info->data.inoMode & 0xF000) | (mode & 0x0FFF);
	data->info->data.inoChangeTime = time();
	data->info->dirty = 1;
	return 0;
};

static int gxfs_fchown(File *fp, uid_t uid, gid_t gid)
{
	FileData *data = (FileData*) fp->fsdata;
	data->info->data.inoOwner = (uint16_t) uid;
	data->info->data.inoGroup = (uint16_t) gid;
	data->info->data.inoChangeTime = time();
	data->info->dirty = 1;
	return 0;
};

static void gxfs_prefetch(FileData *data, uint64_t off)
{
	if (data->prefetchDirty)
	{
		gxfsWriteInode(data->info, data->prefetchBuf, data->prefetchSize, data->prefetchOff);
		data->prefetchDirty = 0;
	};
	
	memset(data->prefetchBuf, 0, PREFETCH_SIZE);
	data->prefetchSize = gxfsReadInode(data->info, data->prefetchBuf, PREFETCH_SIZE, off);
	data->prefetchOff = off;
};

static void gxfs_truncate(File *fp, off_t len)
{
	FileData *data = (FileData*) fp->fsdata;
	gxfsResize(data->info, (uint64_t) len);
};

static ssize_t gxfs_pread(File *fp, void *buffer, size_t size, off_t pos)
{
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->lock);
	
	ssize_t result = 0;
	char *put = (char*) buffer;
	while (size > 0)
	{
		if ((pos < data->prefetchOff) || ((pos+size) >= (data->prefetchOff+data->prefetchSize)))
		{
			gxfs_prefetch(data, pos);
			
			if (data->prefetchSize == 0)
			{
				break;
			};
		};
		
		size_t maxRead = data->prefetchSize - (pos - data->prefetchOff);
		size_t readNow = size;
		if (readNow > maxRead) readNow = maxRead;
		
		memcpy(put, &data->prefetchBuf[pos - data->prefetchOff], readNow);
		put += readNow;
		size -= readNow;
		pos += readNow;
		result += readNow;
	};
	
	semSignal(&data->lock);
	return result;
	
	//return gxfsReadInode(data->info, buffer, size, pos);
};

static ssize_t gxfs_pwrite(File *fp, const void *buffer, size_t size, off_t pos)
{
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->lock);
	
	ssize_t result = 0;
	const char *scan = (const char*) buffer;
	while (size > 0)
	{
		if ((pos < data->prefetchOff) || (pos >= (data->prefetchOff + data->prefetchSize)))
		{
			gxfs_prefetch(data, pos);
		};
		
		size_t maxWrite = PREFETCH_SIZE - (pos - data->prefetchOff);
		size_t writeNow = size;
		if (writeNow > maxWrite) writeNow = maxWrite;
		
		size_t sizeNeeded = pos + writeNow - data->prefetchOff;
		if (data->prefetchSize < sizeNeeded) data->prefetchSize = sizeNeeded;
		
		memcpy(&data->prefetchBuf[pos - data->prefetchOff], scan, writeNow);
		data->prefetchDirty = 1;
		
		scan += writeNow;
		size -= writeNow;
		pos += writeNow;
		result += writeNow;
	};
	
	semSignal(&data->lock);
	return result;
	
	//return gxfsWriteInode(data->info, buffer, size, pos);
};

static off_t gxfs_seek(File *fp, off_t offset, int whence)
{
	FileData *data = (FileData*) fp->fsdata;
	
	semWait(&data->info->lock);
	off_t pos = data->pos;
	
	switch (whence)
	{
	case SEEK_CUR:
		pos += offset;
		break;
	case SEEK_SET:
		pos = offset;
		break;
	case SEEK_END:
		pos = data->info->data.inoSize + offset;
		break;
	};
	
	data->pos = pos;
	semSignal(&data->info->lock);
	
	return pos;
};

static ssize_t gxfs_read(File *fp, void *buffer, size_t size)
{
	FileData *data = (FileData*) fp->fsdata;
	ssize_t ret = fp->pread(fp, buffer, size, data->pos);
	if (ret == -1) return -1;
	data->pos += ret;
	return ret;
};

static ssize_t gxfs_write(File *fp, const void *buffer, size_t size)
{
	FileData *data = (FileData*) fp->fsdata;
	ssize_t ret = fp->pwrite(fp, buffer, size, data->pos);
	if (ret == -1) return -1;
	data->pos += ret;
	return ret;
};

static void gxfs_fsync(File *fp)
{
	FileData *data = (FileData*) fp->fsdata;
	if (data->prefetchDirty)
	{
		gxfsWriteInode(data->info, data->prefetchBuf, data->prefetchSize, data->prefetchOff);
	};
	gxfsFlushInode(data->info);
	
	GXFS *gxfs = data->info->fs;
	if (gxfs->fp->fsync != NULL) gxfs->fp->fsync(gxfs->fp);
};

int gxfsOpenFile(GXFS *gxfs, uint64_t ino, File *fp, size_t filesz)
{
	InodeInfo *info = gxfsGetInode(gxfs, ino);
	if (info == NULL) return -1;
	
	__sync_fetch_and_add(&info->data.inoLinks, 1);
	
	FileData *data = NEW(FileData);
	data->info = info;
	data->pos = 0;
	semInit(&data->lock);
	data->prefetchSize = 0;
	data->prefetchDirty = 0;
	data->prefetchOff = 0;
	memset(data->prefetchBuf, 0, PREFETCH_SIZE);
	
	fp->fsdata = data;
	fp->read = gxfs_read;
	fp->write = gxfs_write;
	fp->seek = gxfs_seek;
	fp->close = gxfs_close;
	fp->dup = gxfs_dup;
	fp->fstat = gxfs_fstat;
	fp->fchmod = gxfs_fchmod;
	fp->fchown = gxfs_fchown;
	fp->truncate = gxfs_truncate;
	fp->pread = gxfs_pread;
	fp->pwrite = gxfs_pwrite;
	fp->fsync = gxfs_fsync;
	
	return 0;
};
