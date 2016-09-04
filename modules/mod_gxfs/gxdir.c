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

#include "gxfs.h"
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/time.h>
#include <glidix/string.h>

static void gxdir_close(Dir *dir)
{
	GXDir *gxdir = (GXDir*) dir->fsdata;
	semWait(&gxdir->gxfs->sem);

	gxdir->gxfs->numOpenInodes--;
	semSignal(&gxdir->gxfs->sem);
	kfree(dir->fsdata);
};

static int gxdir_next(Dir *dir)
{
	GXDir *gxdir = (GXDir*) dir->fsdata;
	semWait(&gxdir->gxfs->sem);

	if ((gxdir->index++) == gxdir->count)
	{
		semSignal(&gxdir->gxfs->sem);
		return -1;
	};

	gxdir->offCurrent = gxdir->gxino.pos;

	char buffer[257];
	GXReadInode(&gxdir->gxino, buffer, gxdir->szNext);
	buffer[gxdir->szNext] = 0;

	gxfsDirent *ent = (gxfsDirent*) buffer;
	gxdir->szNext = ent->deNextSz;
	if (ent->deNameLen > 127)
	{
		panic("found a directory entry with deNameLen over 127; aborting!");
	};

	strcpy(dir->dirent.d_name, ent->deName);
	dir->dirent.d_ino = ent->deInode;
	//kprintf_debug("NEXT INODE : %d\n", ent->deInode);
	if (dir->dirent.d_ino == 0) dir->dirent.d_name[0] = 0;

	//kprintf_debug("NEXT: %s (%d)\n", dir->dirent.d_name, dir->dirent.d_ino);

	semSignal(&gxdir->gxfs->sem);
	return 0;
};

static void gxdir_getstat(Dir *dir)
{
	GXDir *gxdir = (GXDir*) dir->fsdata;
	semWait(&gxdir->gxfs->sem);

	gxfsInode inode;
	GXInode gxino;
	GXOpenInode(gxdir->gxfs, &gxino, dir->dirent.d_ino);
	GXReadInodeHeader(&gxino, &inode);

	dir->stat.st_dev = 0;
	dir->stat.st_ino = dir->dirent.d_ino;
	dir->stat.st_mode = inode.inoMode;
	dir->stat.st_nlink = inode.inoLinks;
	dir->stat.st_uid = inode.inoOwner;
	dir->stat.st_gid = inode.inoGroup;
	dir->stat.st_rdev = 0;
	dir->stat.st_size = inode.inoSize;
	if (dir->stat.st_mode & VFS_MODE_DIRECTORY)
	{
		dir->stat.st_size = sizeof(struct dirent) * (inode.inoLinks - 1);
	};
	dir->stat.st_blksize = gxdir->gxfs->cis.cisBlockSize;
	dir->stat.st_blocks = 0;

	// symlinks take up no blocks; for everything else, count up the blocks
	// (in fact, the link path is written on top of the fragment table!)
	// who cares about XFTs these days anyway
	if ((inode.inoMode & 0xF000) != 0x5000)
	{
		int i;
		for (i=0; i<14; i++)
		{
			dir->stat.st_blocks += inode.inoFrags[i].fExtent;
		};
	};

	dir->stat.st_atime = inode.inoATime;
	dir->stat.st_ctime = inode.inoCTime;
	dir->stat.st_mtime = inode.inoMTime;
	
	dir->stat.st_ixperm = inode.inoIXPerm;
	dir->stat.st_oxperm = inode.inoOXPerm;
	dir->stat.st_dxperm = inode.inoDXPerm;

	semSignal(&gxdir->gxfs->sem);
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
	GXReadInodeHeader(&gxino, &inode);

	inode.inoMode = (inode.inoMode & 0xF000) | (mode & 0x0FFF);
	inode.inoCTime = now;
	GXWriteInodeHeader(&gxino, &inode);

	dir->stat.st_mode = (mode_t) inode.inoMode;
	dir->stat.st_ctime = inode.inoCTime;

	//if (gxdir->gxfs->fp->fsync != NULL) gxdir->gxfs->fp->fsync(gxdir->gxfs->fp);

	semSignal(&gxdir->gxfs->sem);
	return 0;
};

static int gxdir_chxperm(Dir *dir, uint64_t ixperm, uint64_t oxperm, uint64_t dxperm)
{
	time_t now = time();

	GXDir *gxdir = (GXDir*) dir->fsdata;
	semWait(&gxdir->gxfs->sem);

	//kprintf_debug("gxfs: chmod on inode %d\n", dir->dirent.d_ino);
	GXInode gxino;
	GXOpenInode(gxdir->gxfs, &gxino, dir->dirent.d_ino);

	gxfsInode inode;
	GXReadInodeHeader(&gxino, &inode);

	//inode.inoMode = (inode.inoMode & 0xF000) | (mode & 0x0FFF);
	if (ixperm != XP_NCHG) inode.inoIXPerm = ixperm;
	if (oxperm != XP_NCHG) inode.inoOXPerm = oxperm;
	if (dxperm != XP_NCHG) inode.inoDXPerm = dxperm;
	inode.inoCTime = now;
	GXWriteInodeHeader(&gxino, &inode);

	dir->stat.st_ixperm = inode.inoIXPerm;
	dir->stat.st_oxperm = inode.inoOXPerm;
	dir->stat.st_dxperm = inode.inoDXPerm;
	dir->stat.st_ctime = inode.inoCTime;

	//if (gxdir->gxfs->fp->fsync != NULL) gxdir->gxfs->fp->fsync(gxdir->gxfs->fp);

	semSignal(&gxdir->gxfs->sem);
	return 0;
};

static int gxdir_chown(Dir *dir, uid_t uid, gid_t gid)
{
	time_t now = time();

	GXDir *gxdir = (GXDir*) dir->fsdata;
	semWait(&gxdir->gxfs->sem);

	//kprintf_debug("gxfs: chmod on inode %d\n", dir->dirent.d_ino);
	GXInode gxino;
	GXOpenInode(gxdir->gxfs, &gxino, dir->dirent.d_ino);

	gxfsInode inode;
	GXReadInodeHeader(&gxino, &inode);

	inode.inoOwner = (uint16_t) uid;
	inode.inoGroup = (uint16_t) gid;
	inode.inoCTime = now;
	GXWriteInodeHeader(&gxino, &inode);

	dir->stat.st_mode = (mode_t) inode.inoMode;
	dir->stat.st_ctime = inode.inoCTime;

	//if (gxdir->gxfs->fp->fsync != NULL) gxdir->gxfs->fp->fsync(gxdir->gxfs->fp);

	semSignal(&gxdir->gxfs->sem);
	return 0;
};

static int gxdir_mkdir(Dir *dir, const char *name, mode_t mode, uid_t uid, gid_t gid)
{
	time_t now = time();

	GXDir *gxdir = (GXDir*) dir->fsdata;
	///GXFileSystem *gxfs = gxdir->gxfs;
	semWait(&gxdir->gxfs->sem);

	//kprintf_debug("gxfs: mkdir '%s' in inode %d\n", name, gxdir->gxino.ino);

	gxfsInode newInode;
	memset(&newInode, 0, sizeof(gxfsInode));
	GXInode gxNewInode;
	ino_t newInodeNumber = GXCreateInode(gxdir->gxfs, &gxNewInode, gxdir->gxino.ino);

	if (newInodeNumber == 0)
	{
		//kprintf_debug("gxfs: mkdir: out of inodes\n");
		semSignal(&gxdir->gxfs->sem);
		return -1;
	};

	newInode.inoMode = (uint16_t) (mode | VFS_MODE_DIRECTORY);
	newInode.inoSize = 0;
	newInode.inoLinks = 1;
	newInode.inoCTime = now;
	newInode.inoATime = now;
	newInode.inoMTime = now;
	newInode.inoOwner = (uint16_t) uid;
	newInode.inoGroup = (uint16_t) gid;
	memset(&newInode.inoFrags, 0, sizeof(gxfsFragment)*14);
	newInode.inoExFrag = 0;
	GXWriteInodeHeader(&gxNewInode, &newInode);

	gxfsDirHeader emptyhead;
	emptyhead.dhCount = 1;		// the "pseudoentry"
	emptyhead.dhFirstSz = 10;		// a nameless file
	GXWriteInode(&gxNewInode, &emptyhead, 5);
	gxfsDirent pseudoent;
	pseudoent.deInode = 0;
	pseudoent.deNextSz = 0;
	pseudoent.deNameLen = 0;
	GXWriteInode(&gxNewInode, &pseudoent, 10);

	gxfsInode inode;
	GXReadInodeHeader(&gxdir->gxino, &inode);

	char buffer[257];
	gxfsDirent *ent = (gxfsDirent*) buffer;
	ent->deInode = newInodeNumber;
	ent->deNextSz = 0;
	ent->deNameLen = strlen(name);
	strcpy(ent->deName, name);

	// update the next deNextSz for the last entry (none were added since we reached the end due
	// to the kernel's VFS locking).
	gxdir->gxino.pos = gxdir->offCurrent + 8;
	uint8_t newNextSz = 10 + ent->deNameLen;
	GXWriteInode(&gxdir->gxino, &newNextSz, 1);

	// update the dhCount for this directory.
	gxdir->gxino.pos = 0;
	gxfsDirHeader dhead;
	GXReadInode(&gxdir->gxino, &dhead, sizeof(gxfsDirHeader));
	dhead.dhCount++;
	gxdir->gxino.pos = 0;
	GXWriteInode(&gxdir->gxino, &dhead, sizeof(gxfsDirHeader));

	// finally, write the new entry.
	inode.inoLinks++;					// "inoCount" for dirs
	GXWriteInodeHeader(&gxdir->gxino, &inode);

	gxdir->gxino.pos = inode.inoSize;
	GXWriteInode(&gxdir->gxino, ent, 10 + ent->deNameLen);

	strcpy(dir->dirent.d_name, name);
	dir->dirent.d_ino = newInodeNumber;

	//if (gxfs->fp->fsync != NULL) gxfs->fp->fsync(gxfs->fp);

	semSignal(&gxdir->gxfs->sem);
	return 0;
};

static int gxdir_symlink(Dir *dir, const char *name, const char *path)
{
	time_t now = time();

	GXDir *gxdir = (GXDir*) dir->fsdata;
	//GXFileSystem *gxfs = gxdir->gxfs;
	semWait(&gxdir->gxfs->sem);

	//kprintf_debug("gxfs: mkdir '%s' in inode %d\n", name, gxdir->gxino.ino);

	gxfsInode newInode;
	memset(&newInode, 0, sizeof(gxfsInode));
	GXInode gxNewInode;
	ino_t newInodeNumber = GXCreateInode(gxdir->gxfs, &gxNewInode, gxdir->gxino.ino);

	if (newInodeNumber == 0)
	{
		//kprintf_debug("gxfs: mkdir: out of inodes\n");
		semSignal(&gxdir->gxfs->sem);
		return -1;
	};

	newInode.inoMode = (uint16_t) (0644 | VFS_MODE_LINK);
	newInode.inoSize = 0;
	newInode.inoLinks = 1;
	newInode.inoCTime = now;
	newInode.inoATime = now;
	newInode.inoMTime = now;
	newInode.inoOwner = (uint16_t) getCurrentThread()->creds->euid;
	newInode.inoGroup = (uint16_t) getCurrentThread()->creds->egid;
	//memset(&newInode.inoFrags, 0, sizeof(gxfsFragment)*16);
	strcpy((char*) &newInode.inoFrags, path);
	GXWriteInodeHeader(&gxNewInode, &newInode);

	gxfsInode inode;
	GXReadInodeHeader(&gxdir->gxino, &inode);

	char buffer[257];
	gxfsDirent *ent = (gxfsDirent*) buffer;
	ent->deInode = newInodeNumber;
	ent->deNextSz = 0;
	ent->deNameLen = strlen(name);
	strcpy(ent->deName, name);

	// update the next deNextSz for the last entry (none were added since we reached the end due
	// to the kernel's VFS locking).
	gxdir->gxino.pos = gxdir->offCurrent + 8;
	uint8_t newNextSz = 10 + ent->deNameLen;
	GXWriteInode(&gxdir->gxino, &newNextSz, 1);

	// update the dhCount for this directory.
	gxdir->gxino.pos = 0;
	gxfsDirHeader dhead;
	GXReadInode(&gxdir->gxino, &dhead, sizeof(gxfsDirHeader));
	dhead.dhCount++;
	gxdir->gxino.pos = 0;
	GXWriteInode(&gxdir->gxino, &dhead, sizeof(gxfsDirHeader));

	// finally, write the new entry.
	inode.inoLinks++;					// "inoCount" for dirs
	GXWriteInodeHeader(&gxdir->gxino, &inode);

	gxdir->gxino.pos = inode.inoSize;
	GXWriteInode(&gxdir->gxino, ent, 10 + ent->deNameLen);

	strcpy(dir->dirent.d_name, name);
	dir->dirent.d_ino = newInodeNumber;

	//if (gxfs->fp->fsync != NULL) gxfs->fp->fsync(gxfs->fp);

	semSignal(&gxdir->gxfs->sem);
	return 0;
};

static ssize_t gxdir_readlink(Dir *dir, char *buffer)
{
	GXDir *gxdir = (GXDir*) dir->fsdata;
	semWait(&gxdir->gxfs->sem);

	GXInode gxInode;
	GXOpenInode(gxdir->gxfs, &gxInode, dir->dirent.d_ino);

	gxfsInode inode;
	GXReadInodeHeader(&gxInode, &inode);
	strcpy(buffer, (const char*) inode.inoFrags);

	semSignal(&gxdir->gxfs->sem);

	return strlen(buffer);
};

static int gxdir_link(Dir *dir, const char *name, ino_t ino)
{
	//kprintf_debug("LINK TO INODE: %d\n", ino);
	time_t now = time();

	GXDir *gxdir = (GXDir*) dir->fsdata;
	//GXFileSystem *gxfs = gxdir->gxfs;
	semWait(&gxdir->gxfs->sem);

	gxfsInode inode;
	GXInode gxInode;
	GXOpenInode(gxdir->gxfs, &gxInode, ino);
	GXReadInodeHeader(&gxInode, &inode);
	if (inode.inoLinks == 255)
	{
		semSignal(&gxdir->gxfs->sem);
		return -1;
	};

	inode.inoLinks++;
	inode.inoCTime = now;
	GXWriteInodeHeader(&gxInode, &inode);

	GXReadInodeHeader(&gxdir->gxino, &inode);

	char buffer[257];
	gxfsDirent *ent = (gxfsDirent*) buffer;
	ent->deInode = ino;
	ent->deNextSz = 0;
	ent->deNameLen = strlen(name);
	strcpy(ent->deName, name);

	// update the next deNextSz for the last entry (none were added since we reached the end due
	// to the kernel's VFS locking).
	gxdir->gxino.pos = gxdir->offCurrent + 8;
	uint8_t newNextSz = 10 + ent->deNameLen;
	GXWriteInode(&gxdir->gxino, &newNextSz, 1);

	// update the dhCount for this directory.
	gxdir->gxino.pos = 0;
	gxfsDirHeader dhead;
	GXReadInode(&gxdir->gxino, &dhead, sizeof(gxfsDirHeader));
	dhead.dhCount++;
	gxdir->gxino.pos = 0;
	GXWriteInode(&gxdir->gxino, &dhead, sizeof(gxfsDirHeader));

	// finally, write the new entry.
	inode.inoLinks++;					// "inoCount" for dirs
	GXWriteInodeHeader(&gxdir->gxino, &inode);

	gxdir->gxino.pos = inode.inoSize;
	GXWriteInode(&gxdir->gxino, ent, 10 + ent->deNameLen);

	strcpy(dir->dirent.d_name, name);
	dir->dirent.d_ino = ino;

	//if (gxfs->fp->fsync != NULL) gxfs->fp->fsync(gxfs->fp);

	semSignal(&gxdir->gxfs->sem);
	return 0;
};

static int gxdir_mkreg(Dir *dir, const char *name, mode_t mode, uid_t uid, gid_t gid)
{
	time_t now = time();

	GXDir *gxdir = (GXDir*) dir->fsdata;
	GXFileSystem *gxfs = gxdir->gxfs;
	semWait(&gxdir->gxfs->sem);

	//kprintf_debug("gxfs: mkdir '%s' in inode %d\n", name, gxdir->gxino.ino);

	gxfsInode newInode;
	memset(&newInode, 0, sizeof(gxfsInode));
	GXInode gxNewInode;
	ino_t newInodeNumber = GXCreateInode(gxdir->gxfs, &gxNewInode, gxdir->gxino.ino);

	if (newInodeNumber == 0)
	{
		//kprintf_debug("gxfs: mkdir: out of inodes\n");
		semSignal(&gxdir->gxfs->sem);
		return -1;
	};

	newInode.inoMode = (uint16_t) (mode);
	newInode.inoSize = 0;
	newInode.inoLinks = 1;
	newInode.inoCTime = now;
	newInode.inoATime = now;
	newInode.inoMTime = now;
	newInode.inoOwner = (uint16_t) uid;
	newInode.inoGroup = (uint16_t) gid;
	memset(&newInode.inoFrags, 0, sizeof(gxfsFragment)*14);
	newInode.inoExFrag = 0;
	GXWriteInodeHeader(&gxNewInode, &newInode);

	gxfsInode inode;
	GXReadInodeHeader(&gxdir->gxino, &inode);

	char buffer[257];
	gxfsDirent *ent = (gxfsDirent*) buffer;
	ent->deInode = newInodeNumber;
	ent->deNextSz = 0;
	ent->deNameLen = strlen(name);
	strcpy(ent->deName, name);

	// update the next deNextSz for the last entry (none were added since we reached the end due
	// to the kernel's VFS locking).
	gxdir->gxino.pos = gxdir->offCurrent + 8;
	uint8_t oldSz;
	GXReadInode(&gxdir->gxino, &oldSz, 1);
	if (oldSz != 0)
	{
		gxfs->fp->fsync(gxfs->fp);
		panic("gxfs: mkreg() called but oldSz == %d", (int) (uint32_t) oldSz);
	};
	gxdir->gxino.pos = gxdir->offCurrent + 8;
	uint8_t newNextSz = 10 + ent->deNameLen;
	GXWriteInode(&gxdir->gxino, &newNextSz, 1);

	// update the dhCount for this directory.
	gxdir->gxino.pos = 0;
	gxfsDirHeader dhead;
	GXReadInode(&gxdir->gxino, &dhead, sizeof(gxfsDirHeader));
	dhead.dhCount++;
	gxdir->gxino.pos = 0;
	GXWriteInode(&gxdir->gxino, &dhead, sizeof(gxfsDirHeader));

	// finally, write the new entry.

	inode.inoLinks++;					// "inoCount" for dirs
	GXWriteInodeHeader(&gxdir->gxino, &inode);

	gxdir->gxino.pos = inode.inoSize;
	GXWriteInode(&gxdir->gxino, ent, 10 + ent->deNameLen);

	strcpy(dir->dirent.d_name, name);
	dir->dirent.d_ino = newInodeNumber;

	//if (gxfs->fp->fsync != NULL) gxfs->fp->fsync(gxfs->fp);

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
	if (dir->dirent.d_ino < gxfs->cis.cisFirstDataIno)
	{
		return -1;
	};
	semWait(&gxdir->gxfs->sem);

	gxfsInode parentInode;
	GXReadInodeHeader(&gxdir->gxino, &parentInode);
	parentInode.inoCTime = time();
	parentInode.inoMTime = time();
	parentInode.inoLinks--;					// file count decreases
	GXWriteInodeHeader(&gxdir->gxino, &parentInode);

	// rermove the current entry by setting the inode to 0.
	uint64_t inodeZero = 0;
	gxdir->gxino.pos = gxdir->offCurrent;
	GXWriteInode(&gxdir->gxino, &inodeZero, 8);

#if 0
	if (gxdir->gxino.pos != parentInode.inoSize)
	{
		off_t thisPos = gxdir->gxino.pos - sizeof(struct dirent);
		gxdir->gxino.pos = thisPos;
		struct dirent ent;
		memset(&ent, 0, sizeof(struct dirent));
		GXWriteInode(&gxdir->gxino, &ent, sizeof(struct dirent));
	}
	else
	{
		GXShrinkInode(&gxdir->gxino, sizeof(struct dirent), &parentInode);
		GXWriteInodeHeader(&gxdir->gxino, &parentInode);
	};
#endif

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

	gxfsDirHeader dhead;
	GXReadInode(&gxdir->gxino, &dhead, sizeof(gxfsDirHeader));

	gxdir->szNext = dhead.dhFirstSz;
	gxdir->index = 0;
	gxdir->count = dhead.dhCount;

	dir->close = gxdir_close;
	dir->next = gxdir_next;
	dir->opendir = gxdir_opendir;
	dir->openfile = gxdir_openfile;
	dir->chmod = gxdir_chmod;
	dir->chxperm = gxdir_chxperm;
	dir->chown = gxdir_chown;
	dir->mkdir = gxdir_mkdir;
	dir->mkreg = gxdir_mkreg;
	dir->unlink = gxdir_unlink;
	dir->link = gxdir_link;
	dir->symlink = gxdir_symlink;
	dir->readlink = gxdir_readlink;
	dir->getstat = gxdir_getstat;

	gxfs->numOpenInodes++;
	semSignal(&gxfs->sem);
	if (gxdir_next(dir) == -1)
	{
		return VFS_EMPTY_DIRECTORY;
	};

	return 0;
};
