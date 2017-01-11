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

typedef struct
{
	InodeInfo*			dir;
	DirentInfo*			current;
} DirScanner;

static int gxfs_openfile(Dir *dir, File *fp, size_t filesz)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t ino = dir->dirent.d_ino;
	if (ino == 0) return -1;
	
	return gxfsOpenFile(scanner->dir->fs, ino, fp, filesz);
};

static void gxfs_closedir(Dir *dir)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	__sync_fetch_and_add(&scanner->dir->data.inoLinks, -1);
	gxfsDownrefInode(scanner->dir);
	kfree(scanner);
};

static int gxfs_next(Dir *dir)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	scanner->current = scanner->current->next;
	
	if (scanner->current->next == NULL)
	{
		return -1;
	};
	
	memcpy(&dir->dirent, &scanner->current->ent, sizeof(struct dirent));
	return 0;
};

static int gxfs_chmod(Dir *dir, mode_t mode)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t ino = dir->dirent.d_ino;
	if (ino == 0) return -1;
	
	InodeInfo *info = gxfsGetInode(scanner->dir->fs, ino);
	if (info == NULL) return -1;
	
	info->data.inoMode = (info->data.inoMode & 0xF000) | (mode & 0x0FFF);
	info->data.inoChangeTime = time();
	info->dirty = 1;
	gxfsDownrefInode(info);
	
	return 0;
};

static int gxfs_chown(Dir *dir, uid_t uid, gid_t gid)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t ino = dir->dirent.d_ino;
	if (ino == 0) return -1;
	
	InodeInfo *info = gxfsGetInode(scanner->dir->fs, ino);
	if (info == NULL) return -1;
	
	info->data.inoOwner = (uint16_t) uid;
	info->data.inoGroup = (uint16_t) gid;
	info->data.inoChangeTime = time();
	info->dirty = 1;
	gxfsDownrefInode(info);
	
	return 0;
};

static int gxfs_utime(Dir *dir, time_t atime, time_t mtime)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t ino = dir->dirent.d_ino;
	if (ino == 0) return -1;
	
	InodeInfo *info = gxfsGetInode(scanner->dir->fs, ino);
	if (info == NULL) return -1;
	
	info->data.inoAccessTime = atime;
	info->data.inoModTime = mtime;
	info->data.inoChangeTime = time();
	info->dirty = 1;
	gxfsDownrefInode(info);
	
	return 0;
};

static int gxfs_chxperm(Dir *dir, uint64_t ixperm, uint64_t oxperm, uint64_t dxperm)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t ino = dir->dirent.d_ino;
	if (ino == 0) return -1;
	
	InodeInfo *info = gxfsGetInode(scanner->dir->fs, ino);
	if (info == NULL) return -1;
	
	if (ixperm != XP_NCHG) info->data.inoIXPerm = ixperm;
	if (oxperm != XP_NCHG) info->data.inoOXPerm = oxperm;
	if (dxperm != XP_NCHG) info->data.inoDXPerm = dxperm;
	info->data.inoChangeTime = time();
	info->dirty = 1;
	gxfsDownrefInode(info);
	
	return 0;
};

static void gxfs_getstat(Dir *dir)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t ino = dir->dirent.d_ino;
	InodeInfo *targetInfo = gxfsGetInode(scanner->dir->fs, ino);
	if (targetInfo == NULL)
	{
		return;	// ???
	};
	
	semWait(&targetInfo->lock);
	dir->stat.st_ino = ino;
	dir->stat.st_mode = targetInfo->data.inoMode;
	dir->stat.st_nlink = targetInfo->data.inoLinks;
	dir->stat.st_uid = targetInfo->data.inoOwner;
	dir->stat.st_gid = targetInfo->data.inoGroup;
	dir->stat.st_size = targetInfo->data.inoSize;
	dir->stat.st_blksize = 512;
	dir->stat.st_blocks = (1 << (6 * targetInfo->data.inoTreeDepth));
	dir->stat.st_atime = targetInfo->data.inoAccessTime;
	dir->stat.st_mtime = targetInfo->data.inoModTime;
	dir->stat.st_ctime = targetInfo->data.inoChangeTime;
	dir->stat.st_ixperm = targetInfo->data.inoIXPerm;
	dir->stat.st_oxperm = targetInfo->data.inoOXPerm;
	dir->stat.st_dxperm = targetInfo->data.inoDXPerm;
	dir->stat.st_btime = targetInfo->data.inoBirthTime;
	semSignal(&targetInfo->lock);
	
	gxfsDownrefInode(targetInfo);
};

static int gxfs_opendir(Dir *me, Dir *dir, size_t szdir)
{
	DirScanner *scanner = (DirScanner*) me->fsdata;
	
	uint64_t ino = me->dirent.d_ino;
	if (ino == 0) return VFS_IO_ERROR;
	
	return gxfsOpenDir(scanner->dir->fs, ino, dir, szdir);
};

static int gxfs_mkdir(Dir *dir, const char *name, mode_t mode, uid_t uid, gid_t gid)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t blkInode = gxfsAllocBlock(scanner->dir->fs);
	if (blkInode == 0)
	{
		return VFS_NO_SPACE;
	};
	
	uint64_t blkData = gxfsAllocBlock(scanner->dir->fs);
	if (blkData == 0)
	{
		gxfsFreeBlock(scanner->dir->fs, blkInode);
		return VFS_NO_SPACE;
	};
	
	gxfsZeroBlock(scanner->dir->fs, blkData);
	
	uint64_t now = time();
	
	GXFS_Inode inode;
	memset(&inode, 0, 512);
	inode.inoOwner = (uint16_t) uid;
	inode.inoGroup = (uint16_t) gid;
	inode.inoMode = (uint16_t) mode | VFS_MODE_DIRECTORY;
	inode.inoTreeDepth = 0;
	inode.inoLinks = 1;
	inode.inoSize = 0;
	inode.inoBirthTime = now;
	inode.inoChangeTime = now;
	inode.inoModTime = now;
	inode.inoAccessTime = now;
	inode.inoRoot = blkData;
	
	scanner->dir->fs->fp->pwrite(scanner->dir->fs->fp, &inode, 512, 0x200000 + (blkInode << 9));
	
	DirentInfo *endptr = NEW(DirentInfo);
	endptr->next = NULL;
	
	scanner->current->next = endptr;
	scanner->current->opt = 0x01 | ((9 + strlen(name) + 0xF) & (~0xF));
	scanner->current->ent.d_ino = blkInode;
	strcpy(scanner->current->ent.d_name, name);
	
	scanner->dir->dirDirty = 1;
	
	__sync_fetch_and_add(&scanner->dir->data.inoLinks, 1);
	scanner->dir->data.inoModTime = now;
	scanner->dir->dirty = 1;
	
	return 0;
};

static int gxfs_mkreg(Dir *dir, const char *name, mode_t mode, uid_t uid, gid_t gid)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t blkInode = gxfsAllocBlock(scanner->dir->fs);
	if (blkInode == 0)
	{
		return VFS_NO_SPACE;
	};
	
	uint64_t blkData = gxfsAllocBlock(scanner->dir->fs);
	if (blkData == 0)
	{
		gxfsFreeBlock(scanner->dir->fs, blkInode);
		return VFS_NO_SPACE;
	};
	
	gxfsZeroBlock(scanner->dir->fs, blkData);
	
	uint64_t now = time();
	
	GXFS_Inode inode;
	memset(&inode, 0, 512);
	inode.inoOwner = (uint16_t) uid;
	inode.inoGroup = (uint16_t) gid;
	inode.inoMode = (uint16_t) mode;
	inode.inoTreeDepth = 0;
	inode.inoLinks = 1;
	inode.inoSize = 0;
	inode.inoBirthTime = now;
	inode.inoChangeTime = now;
	inode.inoModTime = now;
	inode.inoAccessTime = now;
	inode.inoRoot = blkData;
	
	scanner->dir->fs->fp->pwrite(scanner->dir->fs->fp, &inode, 512, 0x200000 + (blkInode << 9));
	
	DirentInfo *endptr = NEW(DirentInfo);
	endptr->next = NULL;
	
	scanner->current->next = endptr;
	scanner->current->opt = (9 + strlen(name) + 0xF) & (~0xF);
	scanner->current->ent.d_ino = blkInode;
	strcpy(scanner->current->ent.d_name, name);
	scanner->dir->dirDirty = 1;
	
	dir->dirent.d_ino = blkInode;
	strcpy(dir->dirent.d_name, name);
	
	__sync_fetch_and_add(&scanner->dir->data.inoLinks, 1);
	scanner->dir->data.inoModTime = now;
	scanner->dir->dirty = 1;
	
	return 0;
};

static int gxfs_unlink(Dir *dir)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t ino = dir->dirent.d_ino;
	if (ino == 0) return -1;
	
	scanner->dir->data.inoModTime = time();
	__sync_fetch_and_add(&scanner->dir->data.inoLinks, -1);
	dir->dirent.d_ino = 0;
	scanner->current->ent.d_ino = 0;
	scanner->dir->dirty = 1;
	scanner->dir->dirDirty = 1;
	
	InodeInfo *info = gxfsGetInode(scanner->dir->fs, ino);
	if (info == NULL) return 0;
	
	__sync_fetch_and_add(&info->data.inoLinks, -1);
	info->dirty = 1;
	gxfsDownrefInode(info);
	
	return 0;
};

static int gxfs_link(Dir *dir, const char *name, ino_t ino)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t now = time();
	
	InodeInfo *info = gxfsGetInode(scanner->dir->fs, ino);
	if (info == NULL) return -1;
	
	__sync_fetch_and_add(&info->data.inoLinks, 1);
	info->data.inoChangeTime = now;
	info->dirty = 1;
	gxfsDownrefInode(info);
	
	DirentInfo *endptr = NEW(DirentInfo);
	endptr->next = NULL;
	
	scanner->current->next = endptr;
	scanner->current->opt = (9 + strlen(name) + 0xF) & (~0xF);
	scanner->current->ent.d_ino = ino;
	strcpy(scanner->current->ent.d_name, name);
	scanner->dir->dirDirty = 1;
	
	dir->dirent.d_ino = ino;
	strcpy(dir->dirent.d_name, name);
	
	__sync_fetch_and_add(&scanner->dir->data.inoLinks, 1);
	scanner->dir->data.inoModTime = now;
	scanner->dir->dirty = 1;
	
	return 0;
};

static int gxfs_symlink(Dir *dir, const char *name, const char *path)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t blkInode = gxfsAllocBlock(scanner->dir->fs);
	if (blkInode == 0)
	{
		return VFS_NO_SPACE;
	};
	
	uint64_t now = time();
	
	GXFS_Inode inode;
	memset(&inode, 0, 512);
	inode.inoMode = VFS_MODE_LINK;
	inode.inoTreeDepth = 0;
	inode.inoLinks = 1;
	inode.inoSize = 0;
	inode.inoBirthTime = now;
	inode.inoChangeTime = now;
	inode.inoModTime = now;
	inode.inoAccessTime = now;
	strcpy(inode.inoPath, path);
	
	scanner->dir->fs->fp->pwrite(scanner->dir->fs->fp, &inode, 512, 0x200000 + (blkInode << 9));
	
	DirentInfo *endptr = NEW(DirentInfo);
	endptr->next = NULL;
	
	scanner->current->next = endptr;
	scanner->current->opt = 0x05 | ((9 + strlen(name) + 0xF) & (~0xF));
	scanner->current->ent.d_ino = blkInode;
	strcpy(scanner->current->ent.d_name, name);
	
	scanner->dir->dirDirty = 1;
	
	__sync_fetch_and_add(&scanner->dir->data.inoLinks, 1);
	scanner->dir->data.inoModTime = now;
	scanner->dir->dirty = 1;
	
	return 0;
};

static ssize_t gxfs_readlink(Dir *dir, char *buffer)
{
	DirScanner *scanner = (DirScanner*) dir->fsdata;
	
	uint64_t ino = dir->dirent.d_ino;
	InodeInfo *info = gxfsGetInode(scanner->dir->fs, ino);
	if (info == NULL) return -1;
	
	strcpy(buffer, info->data.inoPath);
	gxfsDownrefInode(info);
	
	return strlen(buffer);
};

int gxfsOpenDir(GXFS *gxfs, uint64_t ino, Dir *dir, size_t szdir)
{
	InodeInfo *info = gxfsGetInode(gxfs, ino);
	if (info == NULL) return VFS_IO_ERROR;
	
	semWait(&info->semDir);
	
	if (info->dir == NULL)
	{
		uint64_t pos = 0;
		DirentInfo **putNext = &info->dir;
		
		while (pos < info->data.inoSize)
		{
			GXFS_Dirent dirent;
			
			if (gxfsReadInode(info, &dirent, 9, pos) != 9)
			{
				break;
			};

			DirentInfo *ent = NEW(DirentInfo);
			memset(ent, 0, sizeof(DirentInfo));
			
			size_t entsz = dirent.deOpt & 0xF0;
			size_t namesz = entsz - 9;
			if (namesz > 127) namesz = 127;		// remove the extra padding
			
			ent->opt = dirent.deOpt;
			ent->ent.d_ino = dirent.deInode;
			if (gxfsReadInode(info, ent->ent.d_name, namesz, pos+9) != namesz)
			{
				kfree(ent);
				break;
			};

			*putNext = ent;
			putNext = &ent->next;
			
			pos += entsz;
		};
		
		// add the "end pointer"
		DirentInfo *end = NEW(DirentInfo);
		end->next = NULL;
		*putNext = end;
	};
	
	semSignal(&info->semDir);
	
	DirScanner *scanner = NEW(DirScanner);
	scanner->dir = info;
	scanner->current = info->dir;
	
	dir->fsdata = scanner;
	dir->openfile = gxfs_openfile;
	dir->opendir = gxfs_opendir;
	dir->close = gxfs_closedir;
	dir->next = gxfs_next;
	dir->chmod = gxfs_chmod;
	dir->chown = gxfs_chown;
	dir->mkdir = gxfs_mkdir;
	dir->mkreg = gxfs_mkreg;
	dir->unlink = gxfs_unlink;
	dir->link = gxfs_link;
	dir->symlink = gxfs_symlink;
	dir->readlink = gxfs_readlink;
	dir->getstat = gxfs_getstat;
	dir->utime = gxfs_utime;
	dir->chxperm = gxfs_chxperm;
	
	info->data.inoAccessTime = time();
	__sync_fetch_and_add(&info->data.inoLinks, 1);
	info->dirty = 1;

	if (scanner->current->next == NULL)
	{
		// end directory pointer
		return VFS_EMPTY_DIRECTORY;
	}
	else
	{
		memcpy(&dir->dirent, &scanner->current->ent, sizeof(struct dirent));
	};
	
	return 0;
};

void gxfsFlushDir(InodeInfo *info)
{
	uint64_t pos = 0;
	
	DirentInfo *dirent = info->dir;
	while (dirent != NULL)
	{
		if (dirent->next != NULL)
		{
			if (dirent->ent.d_ino != 0)
			{
				// not the end pointer
				char buffer[256];
				memset(buffer, 0, 256);
			
				GXFS_Dirent *ent = (GXFS_Dirent*) buffer;
				ent->deInode = dirent->ent.d_ino;
				ent->deOpt = dirent->opt;
				strcpy(ent->name, dirent->ent.d_name);
			
				gxfsWriteInode(info, buffer, (size_t) ent->deOpt & 0xF0, pos);
				pos += (size_t) ent->deOpt & 0xF0;
			};
		};
		
		DirentInfo *next = dirent->next;
		kfree(dirent);
		dirent = next;
	};
	
	gxfsResize(info, pos);
};
