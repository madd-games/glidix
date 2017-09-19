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
#include <glidix/ftree.h>

#include "gxfs.h"

typedef struct
{
	InodeInfo *info;
	off_t pos;
} FileData;

static void gxfs_close(File *fp)
{
	FileData *data = (FileData*) fp->fsdata;
	__sync_fetch_and_add(&data->info->data.inoLinks, -1);
	data->info->dirty = 1;
	ftDown(data->info->ft);
	gxfsDownrefInode(data->info);
	kfree(data);
};

static int gxfs_dup(File *me, File *fp, size_t filesz)
{
	FileData *data = (FileData*) me->fsdata;
	memcpy(fp, me, filesz);
	
	FileData *newData = NEW(FileData);
	__sync_fetch_and_add(&data->info->refcount, 1);
	__sync_fetch_and_add(&data->info->data.inoLinks, 1);
	ftUp(data->info->ft);
	
	memcpy(newData, data, sizeof(FileData));
	fp->fsdata = newData;
	
	return 0;
};

static int gxfs_fstat(File *fp, struct stat *st)
{
	FileData *data = (FileData*) fp->fsdata;
	
	mutexLock(&data->info->lock);
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
	mutexUnlock(&data->info->lock);
	
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

static void gxfs_truncate(File *fp, off_t len)
{
	FileData *data = (FileData*) fp->fsdata;

	gxfsResize(data->info, (uint64_t) len);
	ftTruncate(data->info->ft, (size_t) len);
};

static off_t gxfs_seek(File *fp, off_t offset, int whence)
{
	FileData *data = (FileData*) fp->fsdata;
	
	mutexLock(&data->info->lock);
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
	mutexUnlock(&data->info->lock);
	
	return pos;
};

static void gxfs_fsync(File *fp)
{
	FileData *data = (FileData*) fp->fsdata;
	gxfsFlushInode(data->info);
	
	GXFS *gxfs = data->info->fs;
	if (gxfs->fp->fsync != NULL) gxfs->fp->fsync(gxfs->fp);
};

static FileTree* gxfs_tree(File *fp)
{
	FileData *data = (FileData*) fp->fsdata;
	ftUp(data->info->ft);
	return data->info->ft;
};

int gxfsOpenFile(GXFS *gxfs, uint64_t ino, File *fp, size_t filesz)
{
	InodeInfo *info = gxfsGetInode(gxfs, ino);
	if (info == NULL) return -1;
	
	__sync_fetch_and_add(&info->data.inoLinks, 1);
	
	FileData *data = NEW(FileData);
	data->info = info;
	data->pos = 0;
	ftUp(info->ft);
	
	fp->fsdata = data;
	fp->seek = gxfs_seek;
	fp->close = gxfs_close;
	fp->dup = gxfs_dup;
	fp->fstat = gxfs_fstat;
	fp->fchmod = gxfs_fchmod;
	fp->fchown = gxfs_fchown;
	fp->truncate = gxfs_truncate;
	fp->fsync = gxfs_fsync;
	fp->tree = gxfs_tree;
	
	return 0;
};
