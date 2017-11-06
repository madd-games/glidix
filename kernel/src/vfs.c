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

#include <glidix/vfs.h>
#include <glidix/console.h>
#include <glidix/string.h>
#include <glidix/mount.h>
#include <glidix/memory.h>
#include <glidix/sched.h>
#include <glidix/spinlock.h>
#include <glidix/semaphore.h>
#include <glidix/errno.h>
#include <glidix/mutex.h>

static Semaphore semConst;
static FileSystem* kernelRootFS;
static Inode *kernelRootInode;
static Dentry *kernelRootDentry;
static ino_t nextRootIno = 2;

static int rootRegInode(FileSystem *fs, Inode *inode)
{
	inode->ino = __sync_fetch_and_add(&nextRootIno, 1);
	return 0;
};

void vfsInit()
{
	semInit2(&semConst, 1);
	
	// create the "kernel root filesystem". It does not actually appear on the mount table,
	// but is the root of all kernel threads and the initial root of "init"
	kernelRootFS = NEW(FileSystem);
	memset(kernelRootFS, 0, sizeof(FileSystem));
	kernelRootFS->refcount = 1;
	kernelRootFS->numOpenInodes = 0;
	kernelRootFS->fsid = 0;
	kernelRootFS->fstype = "krootfs";
	kernelRootFS->regInode = rootRegInode;
	semInit(&kernelRootFS->lock);
	
	// create the root inode ("/" directory)
	kernelRootInode = vfsCreateInode(kernelRootFS, 0755 | VFS_MODE_DIRECTORY);
	
	// the root dentry
	kernelRootDentry = NEW(Dentry);
	memset(kernelRootDentry, 0, sizeof(Dentry));
	kernelRootDentry->name = strdup("/");
	kernelRootDentry->dir = kernelRootInode;
	vfsUprefInode(kernelRootInode);
	kernelRootDentry->ino = 2;
	kernelRootDentry->key = 0;
	kernelRootDentry->target = kernelRootInode;
	vfsUprefInode(kernelRootInode);
	kernelRootInode->parent = kernelRootDentry;
};

Inode* vfsCreateInode(FileSystem *fs, mode_t mode)
{
	Inode *inode = NEW(Inode);
	memset(inode, 0, sizeof(Inode));
	inode->refcount = 1;
	semInit(&inode->lock);
	inode->fs = fs;
	inode->mode = mode;
	
	if (getCurrentThread()->creds != NULL)
	{
		inode->uid = getCurrentThread()->creds->euid;
		inode->gid = getCurrentThread()->creds->egid;
	};
	
	inode->atime = inode->mtime = inode->ctime = inode->btime = time();
	
	if (fs != NULL)
	{
		semWait(&fs->lock);
		if (fs->regInode != NULL)
		{
			fs->regInode(fs, inode);
		};
		semSignal(&fs->lock);
	};
	
	inode->flags |= VFS_INODE_DIRTY;
	return inode;
};

void vfsUprefInode(Inode *inode)
{
	__sync_fetch_and_add(&inode->refcount, 1);
};

void vfsDownrefInode(Inode *inode)
{
	if (__sync_add_and_fetch(&inode->refcount, -1) == 0)
	{
		if (inode->free != NULL)
		{
			inode->free(inode);
		};
		
		// having a refcount of zero implies that if this is a directory, its dentries are
		// deleted too, so no need to free those.
		if (inode->fs != NULL) vfsDownrefFileSystem(inode->fs);
		if (inode->ft != NULL) ftUncache(inode->ft);
		kfree(inode->target);
		kfree(inode);
	};
};

void vfsUprefFileSystem(FileSystem *fs)
{
	__sync_fetch_and_add(&fs->refcount, 1);
};

void vfsDownrefFileSystem(FileSystem *fs)
{
	if (__sync_add_and_fetch(&fs->refcount, -1) == 0)
	{
		kfree(fs->path);
		kfree(fs->image);
		kfree(fs);
	};
};

Dentry* vfsGetDentry(Inode *startdir, const char *path, int *error)
{
	return NULL;
};
