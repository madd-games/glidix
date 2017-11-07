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

Dentry* vfsGetDentry(Inode *startdir, const char *path, int oflags, int *error)
{
	// empty paths are not supposed to be resolved
	if (path[0] == 0)
	{
		if (error != NULL) *error = ENOENT;
		return NULL;
	};
	
	char *pathbuf = strdup(path);
	char *buffer = pathbuf;
	if (buffer[0] == '/')
	{
		// TODO: process root directory
		startdir = kernelRootInode;
		buffer++;
	}
	else if (startdir == NULL)
	{
		// TODO: working directory
		startdir = kernelRootInode;
	};
	
	semWait(&startdir->lock);
	Dentry *currentDent = startdir->parent;
	vfsUprefInode(currentDent->dir);
	semSignal(&startdir->lock);
	
	semWait(&currentDent->dir->lock);
	
	while (1)
	{
		int finalToken;
		const char *token;
		char *slashPos = strstr(buffer, "/");
		if (slashPos == NULL)
		{
			token = buffer;
			finalToken = 1;
		}
		else
		{
			*slashPos = 0;
			token = buffer;
			buffer = slashPos+1;
			finalToken = 0;
		};
		
		if (token[0] == 0)
		{
			if (!finalToken) continue;
			else break;
		}
		else if (strcmp(token, ".") == 0)
		{
			if (!finalToken) continue;
			else break;
		}
		else if (strcmp(token, "..") == 0)
		{
			if (currentDent == kernelRootDentry)
			{
				if (!finalToken) continue;
				else break;
			};
			
			// TODO: check if the directory is the process root directory
			
			Inode *dir = currentDent->dir;
			vfsUprefInode(dir->parent->dir);
			currentDent = dir->parent;
			semSignal(&dir->lock);
			semWait(&currentDent->dir->lock);
			
			if (!finalToken) continue;
			else break;
		};
		
		// load the inode if necessary
		if (currentDent->target == NULL)
		{
			Inode *inode = NEW(Inode);
			memset(inode, 0, sizeof(Inode));
			inode->refcount = 1;
			semInit(&inode->lock);
			inode->fs = currentDent->dir->fs;
			vfsUprefFileSystem(inode->fs);
			inode->parent = currentDent;
			vfsUprefInode(currentDent->dir);
			inode->ino = currentDent->ino;
			
			semWait(&inode->fs->lock);
			int ok = 0;
			if (inode->fs->loadInode != NULL)
			{
				if (inode->fs->loadInode(inode->fs, currentDent, inode) == 0)
				{
					ok = 1;
				};
			};
			semSignal(&inode->fs->lock);
			
			if (!ok)
			{
				vfsDownrefInode(currentDent->dir);
				vfsDownrefFileSystem(inode->fs);
				semSignal(&currentDent->dir->lock);
				vfsDownrefInode(currentDent->dir);
				kfree(inode);
				kfree(pathbuf);
				if (error != NULL) *error = EIO;
				return NULL;
			};
			
			currentDent->target = inode;
		};
		
		Inode *dirnode = currentDent->target;
		vfsUprefInode(dirnode);
		semSignal(&currentDent->dir->lock);
		vfsDownrefInode(currentDent->dir);
		
		semWait(&dirnode->lock);
		
		Dentry *dent;
		for (dent=dirnode->dents; dent!=NULL; dent=dent->next)
		{
			if (strcmp(dent->name, token) == 0)
			{
				break;
			};
		};
		
		if (dent == NULL)
		{
			if (oflags & O_CREAT)
			{
				Dentry *newent = NEW(Dentry);
				memset(newent, 0, sizeof(Dentry));
				newent->name = strdup(token);
				vfsUprefInode(dirnode);
				newent->dir = dirnode;
				// TODO: key allocation
				newent->flags = VFS_DENTRY_TEMP;
				
				if (dirnode->dents == NULL)
				{
					newent->prev = newent->next = NULL;
					dirnode->dents = newent;
				}
				else
				{
					Dentry *last = dirnode->dents;
					while (last->next != NULL) last = last->next;
					newent->prev = last;
					last->next = newent;
					newent->next = NULL;
				};
				
				dent = newent;
			}
			else
			{
				semSignal(&dirnode->lock);
				vfsDownrefInode(dirnode);
				kfree(pathbuf);
				if (error != NULL) *error = ENOENT;
				return NULL;
			};
		}
		else if (oflags & O_EXCL)
		{
			semSignal(&dirnode->lock);
			vfsDownrefInode(dirnode);
			kfree(pathbuf);
			if (error != NULL) *error = EEXIST;
			return NULL;
		};
		
		// only enter directories (or symlinks to them)
		if (!finalToken)
		{
			// TODO: follow symlinks
			if ((dirnode->mode & 0xF000) != VFS_MODE_DIRECTORY)
			{
				semSignal(&dirnode->lock);
				vfsDownrefInode(dirnode);
				kfree(pathbuf);
				if (error != NULL) *error = ENOTDIR;
				return NULL;
			};
		};
		
		currentDent = dent;
		if (finalToken) break;
	};
	
	// TODO: follow symlink if necessary
	return currentDent;
};

void vfsUnlockDentry(Dentry *dent)
{
	semSignal(&dent->dir->lock);
	vfsDownrefInode(dent->dir);
};
