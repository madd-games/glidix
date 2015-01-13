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

#include "gxfs.h"
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/time.h>
#include <glidix/string.h>

static void gxdir_close(Dir *dir)
{
	kfree(dir->fsdata);
};

static int gxdir_next(Dir *dir)
{
	GXDir *gxdir = (GXDir*) dir->fsdata;
	semWait(&gxdir->gxfs->sem);
	ssize_t iread = GXReadInode(&gxdir->gxino, &dir->dirent, sizeof(struct dirent));
	//kprintf_debug("gxfs: directory read: %d bytes\n", iread);
	if (iread < sizeof(struct dirent))
	{
		semSignal(&gxdir->gxfs->sem);
		return -1;
	};

	gxfsInode inode;
	GXInode gxino;
	//kprintf_debug("gxfs: stat on inode %d\n", dir->dirent.d_ino);
	GXOpenInode(gxdir->gxfs, &gxino, dir->dirent.d_ino);
	gxdir->gxfs->fp->seek(gxdir->gxfs->fp, gxino.offset, SEEK_SET);
	vfsRead(gxdir->gxfs->fp, &inode, sizeof(gxfsInode));

	dir->stat.st_dev = 0;
	dir->stat.st_ino = dir->dirent.d_ino;
	dir->stat.st_mode = inode.inoMode;
	dir->stat.st_nlink = inode.inoLinks;
	dir->stat.st_uid = inode.inoOwner;
	dir->stat.st_gid = inode.inoGroup;
	dir->stat.st_rdev = 0;
	dir->stat.st_size = inode.inoSize;
	dir->stat.st_blksize = gxdir->gxfs->cis.cisBlockSize;
	dir->stat.st_blocks = 0;
	int i;
	for (i=0; i<16; i++)
	{
		dir->stat.st_blocks += inode.inoFrags[i].fExtent;
	};
	dir->stat.st_atime = inode.inoATime;
	dir->stat.st_ctime = inode.inoCTime;
	dir->stat.st_mtime = inode.inoMTime;

	semSignal(&gxdir->gxfs->sem);
	return 0;
};

static int gxdir_opendir(Dir *me, Dir *dir, size_t szdir)
{
	GXDir *gxdir = (GXDir*) me->fsdata;
	return GXOpenDir(gxdir->gxfs, dir, me->dirent.d_ino);
};

static int gxdir_openfile(Dir *dir, File *fp, size_t szfp)
{
	GXDir *gxdir = (GXDir*) dir->fsdata;
	if (dir->stat.st_mode & 0xF000) return -1;
	return GXOpenFile(gxdir->gxfs, fp, dir->dirent.d_ino);
};

static int gxdir_chmod(Dir *dir, mode_t mode)
{
	time_t now = time();

	GXDir *gxdir = (GXDir*) dir->fsdata;
	semWait(&gxdir->gxfs->sem);

	//kprintf_debug("gxfs: chmod on inode %d\n", dir->dirent.d_ino);
	GXInode gxino;
	GXOpenInode(gxdir->gxfs, &gxino, dir->dirent.d_ino);

	gxfsInode inode;
	gxdir->gxfs->fp->seek(gxdir->gxfs->fp, gxino.offset, SEEK_SET);
	vfsRead(gxdir->gxfs->fp, &inode, 39);
	gxdir->gxfs->fp->seek(gxdir->gxfs->fp, -39, SEEK_CUR);
	inode.inoMode = (inode.inoMode & 0xF000) | (mode & 0x0FFF);
	inode.inoCTime = now;
	int status = 0;
	if (vfsWrite(gxdir->gxfs->fp, &inode, 39) == -1)
	{
		//kprintf_debug("gxfs: vfsWrite() failed in chmod\n");
		status = -1;
	};
	dir->stat.st_mode = (mode_t) inode.inoMode;
	dir->stat.st_ctime = inode.inoCTime;

	if (gxdir->gxfs->fp->fsync != NULL) gxdir->gxfs->fp->fsync(gxdir->gxfs->fp);

	semSignal(&gxdir->gxfs->sem);
	return status;
};

static int gxdir_chown(Dir *dir, uid_t uid, gid_t gid)
{
	time_t now = time();

	GXDir *gxdir = (GXDir*) dir->fsdata;
	semWait(&gxdir->gxfs->sem);

	//kprintf_debug("gxfs: chown on inode %d\n", dir->dirent.d_ino);
	GXInode gxino;
	GXOpenInode(gxdir->gxfs, &gxino, dir->dirent.d_ino);

	gxfsInode inode;
	gxdir->gxfs->fp->seek(gxdir->gxfs->fp, gxino.offset, SEEK_SET);
	vfsRead(gxdir->gxfs->fp, &inode, 39);
	gxdir->gxfs->fp->seek(gxdir->gxfs->fp, -39, SEEK_CUR);
	inode.inoOwner = (uint16_t) uid;
	inode.inoGroup = (uint16_t) gid;
	inode.inoCTime = now;
	int status = 0;
	if (vfsWrite(gxdir->gxfs->fp, &inode, 39) == -1)
	{
		//kprintf_debug("gxfs: vfsWrite() failed in chown\n");
		status = -1;
	};
	dir->stat.st_uid = uid;
	dir->stat.st_gid = gid;
	dir->stat.st_ctime = inode.inoCTime;

	if (gxdir->gxfs->fp->fsync != NULL) gxdir->gxfs->fp->fsync(gxdir->gxfs->fp);

	semSignal(&gxdir->gxfs->sem);
	return status;
};

static int gxdir_mkdir(Dir *dir, const char *name, mode_t mode, uid_t uid, gid_t gid)
{
	time_t now = time();

	GXDir *gxdir = (GXDir*) dir->fsdata;
	GXFileSystem *gxfs = gxdir->gxfs;
	semWait(&gxdir->gxfs->sem);

	//kprintf_debug("gxfs: mkdir '%s' in inode %d\n", name, gxdir->gxino.ino);

	gxfsInode newInode;
	GXInode gxNewInode;
	ino_t newInodeNumber = GXCreateInode(gxdir->gxfs, &gxNewInode, gxdir->gxino.ino);

	if (newInodeNumber == 0)
	{
		//kprintf_debug("gxfs: mkdir: out of inodes\n");
		semSignal(&gxdir->gxfs->sem);
		return -1;
	};

	gxdir->gxfs->fp->seek(gxdir->gxfs->fp, gxNewInode.offset, SEEK_SET);
	newInode.inoMode = (uint16_t) (mode | VFS_MODE_DIRECTORY);
	newInode.inoSize = 0;
	newInode.inoLinks = 1;
	newInode.inoCTime = now;
	newInode.inoATime = now;
	newInode.inoMTime = now;
	newInode.inoOwner = (uint16_t) uid;
	newInode.inoGroup = (uint16_t) gid;
	memset(&newInode.inoFrags, 0, sizeof(gxfsFragment)*16);
	vfsWrite(gxdir->gxfs->fp, &newInode, sizeof(gxfsInode));

	//kprintf_debug("gxfs: writing directory entry\n");
	struct dirent ent;
	ent.d_ino = newInodeNumber;
	strcpy(ent.d_name, name);
	GXWriteInode(&gxdir->gxino, &ent, sizeof(struct dirent));

	//kprintf_debug("gxfs: updating st_mtime for inode %d (parent directory)\n", gxdir->gxino.ino);
	gxfs->fp->seek(gxfs->fp, gxdir->gxino.offset, SEEK_SET);
	vfsRead(gxfs->fp, &newInode, 39);
	gxfs->fp->seek(gxfs->fp, -39, SEEK_CUR);
	newInode.inoMTime = now;
	vfsWrite(gxfs->fp, &newInode, 39);

	if (gxfs->fp->fsync != NULL) gxfs->fp->fsync(gxfs->fp);

	semSignal(&gxdir->gxfs->sem);
	return 0;
};

static int gxdir_mkreg(Dir *dir, const char *name, mode_t mode, uid_t uid, gid_t gid)
{
	time_t now = time();

	GXDir *gxdir = (GXDir*) dir->fsdata;
	GXFileSystem *gxfs = gxdir->gxfs;
	semWait(&gxdir->gxfs->sem);

	//kprintf_debug("gxfs: mkreg '%s' in inode %d\n", name, gxdir->gxino.ino);

	gxfsInode newInode;
	GXInode gxNewInode;
	ino_t newInodeNumber = GXCreateInode(gxdir->gxfs, &gxNewInode, gxdir->gxino.ino);

	if (newInodeNumber == 0)
	{
		kprintf_debug("gxfs: mkreg: out of inodes\n");
		semSignal(&gxdir->gxfs->sem);
		return -1;
	};

	gxdir->gxfs->fp->seek(gxdir->gxfs->fp, gxNewInode.offset, SEEK_SET);
	newInode.inoMode = (uint16_t) mode;
	newInode.inoSize = 0;
	newInode.inoLinks = 1;
	newInode.inoCTime = now;
	newInode.inoATime = now;
	newInode.inoMTime = now;
	newInode.inoOwner = (uint16_t) uid;
	newInode.inoGroup = (uint16_t) gid;
	memset(&newInode.inoFrags, 0, sizeof(gxfsFragment)*16);
	vfsWrite(gxdir->gxfs->fp, &newInode, sizeof(gxfsInode));

	//kprintf_debug("gxfs: writing directory entry\n");
	struct dirent ent;
	ent.d_ino = newInodeNumber;
	strcpy(ent.d_name, name);
	GXWriteInode(&gxdir->gxino, &ent, sizeof(struct dirent));

	//kprintf_debug("gxfs: updating st_mtime for inode %d (parent directory)\n", gxdir->gxino.ino);
	gxfs->fp->seek(gxfs->fp, gxdir->gxino.offset, SEEK_SET);
	vfsRead(gxfs->fp, &newInode, 39);
	gxfs->fp->seek(gxfs->fp, -39, SEEK_CUR);
	newInode.inoMTime = now;
	vfsWrite(gxfs->fp, &newInode, 39);

	if (gxfs->fp->fsync != NULL) gxfs->fp->fsync(gxfs->fp);

	strcpy(dir->dirent.d_name, name);
	dir->dirent.d_ino = newInodeNumber;
	dir->stat.st_dev = 0;
	dir->stat.st_ino = newInodeNumber;
	dir->stat.st_mode = mode;
	dir->stat.st_nlink = 1;
	dir->stat.st_uid = uid;
	dir->stat.st_gid = gid;
	dir->stat.st_rdev = 0;
	dir->stat.st_size = 0;
	dir->stat.st_blksize = gxfs->cis.cisBlockSize;
	dir->stat.st_blocks = 0;
	dir->stat.st_atime = now;
	dir->stat.st_ctime = now;
	dir->stat.st_mtime = now;

	semSignal(&gxdir->gxfs->sem);
	return 0;
};

static int gxdir_unlink(Dir *dir)
{
	GXDir *gxdir = (GXDir*) dir->fsdata;
	GXFileSystem *gxfs = gxdir->gxfs;
	semWait(&gxdir->gxfs->sem);

	gxfsInode parentInode;
	GXReadInodeHeader(&gxdir->gxino, &parentInode);
	parentInode.inoCTime = time();
	parentInode.inoMTime = time();

	if (gxdir->gxino.pos != parentInode.inoSize)
	{
		// swap this with the last entry so that we can just shrink
		struct dirent lastent;
		off_t thisPos = gxdir->gxino.pos - sizeof(struct dirent);
		gxdir->gxino.pos = parentInode.inoSize - sizeof(struct dirent);
		GXReadInode(&gxdir->gxino, &lastent, sizeof(struct dirent));
		gxdir->gxino.pos = thisPos;
		GXWriteInode(&gxdir->gxino, &lastent, sizeof(struct dirent));
	};
	GXShrinkInode(&gxdir->gxino, sizeof(struct dirent), &parentInode);
	GXWriteInodeHeader(&gxdir->gxino, &parentInode);

	// TODO: file opens should add to inoLinks!
	GXInode gxino;
	GXOpenInode(gxfs, &gxino, dir->dirent.d_ino);
	GXUnlinkInode(&gxino);

	semSignal(&gxdir->gxfs->sem);
	return 0;
};

int GXOpenDir(GXFileSystem *gxfs, Dir *dir, ino_t ino)
{
	semWait(&gxfs->sem);
	//kprintf_debug("GXOpenDir(%d)\n", ino);
	GXDir *gxdir = (GXDir*) kmalloc(sizeof(GXDir));
	dir->fsdata = gxdir;

	gxdir->gxfs = gxfs;
	GXOpenInode(gxfs, &gxdir->gxino, ino);

	dir->close = gxdir_close;
	dir->next = gxdir_next;
	dir->opendir = gxdir_opendir;
	dir->openfile = gxdir_openfile;
	dir->chmod = gxdir_chmod;
	dir->chown = gxdir_chown;
	dir->mkdir = gxdir_mkdir;
	dir->mkreg = gxdir_mkreg;
	dir->unlink = gxdir_unlink;

	semSignal(&gxfs->sem);
	if (gxdir_next(dir) == -1)
	{
		return VFS_EMPTY_DIRECTORY;
	};

	return 0;
};
