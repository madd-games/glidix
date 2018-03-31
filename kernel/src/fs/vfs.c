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

#include <glidix/fs/vfs.h>
#include <glidix/display/console.h>
#include <glidix/util/string.h>
#include <glidix/util/memory.h>
#include <glidix/thread/sched.h>
#include <glidix/thread/spinlock.h>
#include <glidix/thread/semaphore.h>
#include <glidix/util/errno.h>
#include <glidix/thread/mutex.h>

static Semaphore semConst;
static FileSystem* kernelRootFS;
static Inode *kernelRootInode;
static Dentry *kernelRootDentry;
static ino_t nextRootIno = 2;

DentryRef VFS_NULL_DREF = {NULL, NULL};
InodeRef VFS_NULL_IREF = {NULL, NULL};

static int rootfs_load(FileTree *ft, off_t pos, void *buffer)
{
	// do nothing; leave zeroed out
	return 0;
};

static int rootRegInode(FileSystem *fs, Inode *inode)
{
	inode->ino = __sync_fetch_and_add(&nextRootIno, 1);
	
	if ((inode->mode & VFS_MODE_TYPEMASK) == 0)
	{
		// create caches for regular files
		inode->ft = ftCreate(FT_ANON);
		inode->ft->load = rootfs_load;
	};
	
	return 0;
};

void vfsInit()
{
	semInit2(&semConst, 1);
	
	// create the "kernel root filesystem". It does not actually appear on the mount table,
	// but is the root of all kernel threads and the initial root of "init"
	kernelRootFS = NEW(FileSystem);
	memset(kernelRootFS, 0, sizeof(FileSystem));
	kernelRootFS->numMounts = 1;
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

FileSystem* vfsCreateFileSystem(const char *fstype)
{
	static dev_t nextFSID = 1;
	
	FileSystem *fs = NEW(FileSystem);
	memset(fs, 0, sizeof(FileSystem));
	fs->fsid = __sync_fetch_and_add(&nextFSID, 1);
	fs->fstype = fstype;
	semInit(&fs->lock);
	
	return fs;
};

Inode* vfsCreateInode(FileSystem *fs, mode_t mode)
{	
	mode_t umask = 0;
	if (getCurrentThread() != NULL && getCurrentThread()->creds != NULL) umask = getCurrentThread()->creds->umask;
		
	Inode *inode = NEW(Inode);
	memset(inode, 0, sizeof(Inode));
	inode->refcount = 1;
	if (fs == NULL) inode->dups = 1;
	semInit(&inode->lock);
	inode->fs = fs;
	inode->mode = mode & ~umask;
	inode->nextKey = 2;		/* 0 and 1 are for "." and ".." */

	if (getCurrentThread() != NULL && getCurrentThread()->creds != NULL)
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
			if (fs->regInode(fs, inode) != 0)
			{
				semSignal(&fs->lock);
				kfree(inode);
				return NULL;
			};
		}
		else
		{
			panic("inode creation attempted on a filesystem that can't register inodes");
		};
		inode->next = fs->imap;
		if (fs->imap != NULL) fs->imap->prev = inode;
		fs->imap = inode;
		semSignal(&fs->lock);
	}
	else
	{
		static ino_t fsLessInoNext = 10;
		inode->ino = __sync_fetch_and_add(&fsLessInoNext, 1);
	};
	
	inode->flags |= VFS_INODE_DIRTY;
	return inode;
};

void vfsDirtyInode(Inode *inode)
{
	inode->flags |= VFS_INODE_DIRTY;
};

int vfsFlush(Inode *inode)
{
	if (inode->ino == 0)
	{
		// inode has been dropped
		ERRNO = EPERM;
		return -1;
	};
	
	semWait(&inode->lock);
	if (inode->ft != NULL) ftFlush(inode->ft);
	if (inode->flush != NULL)
	{
		if (inode->flush(inode) != 0)
		{
			int error = ERRNO;
			semSignal(&inode->lock);
			return error;
		};
	};
	semSignal(&inode->lock);
	
	return 0;
};

void vfsUprefInode(Inode *inode)
{
	__sync_fetch_and_add(&inode->refcount, 1);
};

void vfsDownrefInode(Inode *inode)
{
	if (__sync_add_and_fetch(&inode->refcount, -1) == 0)
	{
		// make sure we are removed from the inode map
		FileSystem *fs = inode->fs;
		if (fs != NULL && !fs->unmounting)
		{
			semWait(&fs->lock);
			if (inode->prev != NULL) inode->prev->next = inode->next;
			if (inode->next != NULL) inode->next->prev = inode->prev;
			if (fs->imap == inode) fs->imap = inode->next;
			semSignal(&fs->lock);
		};
		
		if (inode->free != NULL)
		{
			inode->free(inode);
		};
		
		// having a refcount of zero implies that if this is a directory, its dentries are
		// deleted too, so no need to free those.
		if (inode->ft != NULL)
		{
			ftUp(inode->ft);
			ftUncache(inode->ft);
			ftDown(inode->ft);
		};
		
		kfree(inode->target);
		kfree(inode);
	};
};

DentryRef vfsGetParentDentry(InodeRef iref)
{
	// first see if we are leaving a mountpoint
	if (iref.top != NULL && iref.inode == iref.top->root)
	{
		// move up the the mountpoint's parent dentry.
		DentryRef dref;
		dref.dent = iref.top->dent;		// reference transferred; do not change refcount
		dref.top = iref.top->prev;

		// remove two references to the inode: the one from "iref" and the one in the mountpoint
		// description
		vfsDownrefInode(iref.inode);
		vfsDownrefInode(iref.top->root);
		
		// release the description
		kfree(iref.top);
		
		// done
		semWait(&dref.dent->dir->lock);
		return dref;
	}
	else
	{
		DentryRef dref;
		dref.dent = iref.inode->parent;
		dref.top = iref.top;
		
		vfsUprefInode(dref.dent->dir);		// we're taking a reference to it
		vfsDownrefInode(iref.inode);
		semWait(&dref.dent->dir->lock);
		return dref;
	};
};

InodeRef vfsGetDentryContainer(DentryRef dref)
{
	InodeRef iref;
	iref.inode = dref.dent->dir;
	iref.top = dref.top;
	semSignal(&iref.inode->lock);
	return iref;
};

void vfsCallInInode(Dentry *dent)
{
	if (dent->target == NULL)
	{
		FileSystem *fs = dent->dir->fs;
		if (fs == NULL)
		{
			panic("dentry cache inconsistent: dentry without cached target inode or backing filesystem");
		};
		
		semWait(&fs->lock);
		int found = 0;
		
		// first see if we have already loaded an inode with this number into the inode map
		Inode *scan;
		for (scan=fs->imap; scan!=NULL; scan=scan->next)
		{
			if (scan->refcount != 0)
			{
				if (scan->ino == dent->ino)
				{
					__sync_fetch_and_add(&scan->dups, 1);
					vfsUprefInode(scan);
					found = 1;
					break;
				};
			};
		};
		
		// if the inode is not in the map yet, load it from disk
		if (!found)
		{
			Inode *inode = NEW(Inode);
			memset(inode, 0, sizeof(Inode));
			inode->refcount = 1;
			inode->dups = 1;
			semInit(&inode->lock);
			inode->fs = fs;
			inode->parent = dent;
			inode->nextKey = 2;
			inode->ino = dent->ino;

			if (fs->loadInode == NULL)
			{
				panic("dentry without cached target inode, and backing filesytem driver cannot load inodes");
			};
		
			if (fs->loadInode(fs, inode) != 0)
			{
				semSignal(&fs->lock);
				kfree(inode);
				return;
			};
			
			inode->next = fs->imap;
			if (fs->imap != NULL) fs->imap->prev = inode;
			fs->imap = inode;

			dent->target = inode;
		}
		else
		{
			dent->target = scan;
		};
		
		semSignal(&fs->lock);
	};
};

InodeRef vfsGetDentryTarget(DentryRef dref)
{
	vfsCallInInode(dref.dent);
	if (dref.dent->target == NULL)
	{
		Inode *dir = dref.dent->dir;
		semSignal(&dir->lock);
		vfsDownrefInode(dir);
		
		InodeRef nulref;
		nulref.top = dref.top;
		nulref.inode = NULL;
		return nulref;
	};
	
	InodeRef iref;
	Inode *target = dref.dent->target;
	vfsUprefInode(target);
	
	iref.inode = target;
	iref.top = dref.top;

	if (dref.dent->flags & VFS_DENTRY_MNTPOINT)
	{
		vfsUprefInode(target);
		vfsUprefInode(dref.dent->dir);
		
		MountPoint *newTop = NEW(MountPoint);
		newTop->prev = iref.top;
		newTop->dent = dref.dent;
		newTop->root = iref.inode;
		iref.top = newTop;
	};
	
	Inode *dir = dref.dent->dir;
	semSignal(&dir->lock);
	vfsDownrefInode(dir);

	return iref;
};

InodeRef vfsGetInode(DentryRef dref, int follow, int *error)
{
	static InodeRef nulref = {NULL, NULL};
	
	if (!follow)
	{
		if (error != NULL) *error = EIO;	// in case the below call fails
		return vfsGetDentryTarget(dref);
	};
	
	InodeRef iref = vfsGetDentryTarget(dref);
	if (iref.inode == NULL)
	{
		vfsUnrefInode(iref);
		if (error != NULL) *error = EIO;
		return nulref;
	};
	
	int limit = VFS_MAX_LINK_DEPTH;
	while ((iref.inode->mode & 0xF000) == VFS_MODE_LINK)
	{
		if (--limit == 0)
		{
			vfsUnrefInode(iref);
			if (error != NULL) *error = ELOOP;
			return nulref;
		};
		
		vfsUprefInode(iref.inode);
		char *target = vfsReadLink(iref.inode);
		if (target == NULL)
		{
			vfsUnrefInode(iref);
			if (error != NULL) *error = EIO;
			return nulref;
		};
		
		dref = vfsGetParentDentry(iref);
		InodeRef diref = vfsGetDentryContainer(dref);
		dref = vfsGetDentry(diref, target, 0, error);
		kfree(target);
		
		if (dref.dent == NULL)
		{
			vfsUnrefDentry(dref);
			return nulref;
		};
		
		iref = vfsGetDentryTarget(dref);
		if (iref.inode == NULL)
		{
			vfsUnrefInode(iref);
			if (error != NULL) *error = EIO;
			return nulref;
		};
	};
	
	return iref;
};

InodeRef vfsGetRoot()
{
	Creds *creds = NULL;
	if (getCurrentThread() != NULL) creds = getCurrentThread()->creds;
	
	if (creds == NULL)
	{
		vfsUprefInode(kernelRootInode);
	
		InodeRef iref;
		iref.inode = kernelRootInode;
		iref.top = NULL;
	
		return iref;
	}
	else
	{
		semWait(&creds->semDir);
		InodeRef iref = vfsCopyInodeRef(creds->rootdir);
		semSignal(&creds->semDir);
		return iref;
	};
};

InodeRef vfsGetCurrentDir()
{
	Creds *creds = NULL;
	if (getCurrentThread() != NULL) creds = getCurrentThread()->creds;
	
	if (creds == NULL)
	{
		return vfsGetRoot();
	}
	else
	{
		semWait(&creds->semDir);
		InodeRef iref = vfsCopyInodeRef(creds->cwd);
		semSignal(&creds->semDir);
		return iref;
	};
};

DentryRef vfsGetChildDentry(InodeRef diref, const char *entname, int create)
{
	// update the access time of the inode
	semWait(&diref.inode->lock);
	diref.inode->atime = time();
	vfsDirtyInode(diref.inode);		// should it be made dirty here?
	semSignal(&diref.inode->lock);
	
	// first the special ones
	if (strcmp(entname, ".") == 0 || entname[0] == 0)
	{
		return vfsGetParentDentry(diref);
	}
	else if (strcmp(entname, "..") == 0)
	{
		InodeRef root = vfsGetRoot();
		if (diref.inode == root.inode)
		{
			vfsUnrefInode(root);
			return vfsGetParentDentry(diref);
		};
		
		vfsUnrefInode(root);
		
		DentryRef parent = vfsGetParentDentry(diref);
		InodeRef up = vfsGetDentryContainer(parent);
		return vfsGetParentDentry(up);
	}
	else
	{
		// for all other names, we must go through the list of dentries and
		// return the named one while locked
		semWait(&diref.inode->lock);
		
		// first check if it already exists
		Dentry *dent;
		for (dent=diref.inode->dents; dent!=NULL; dent=dent->next)
		{
			if (strcmp(dent->name, entname) == 0)
			{
				DentryRef dref;
				dref.dent = dent;
				dref.top = diref.top;
				
				return dref;
			};
		};
		
		// not found; create if needed else fail
		if (create)
		{
			if (!vfsIsAllowed(diref.inode, VFS_ACE_WRITE))
			{
				semSignal(&diref.inode->lock);
				vfsUnrefInode(diref);
				
				DentryRef nulref;
				nulref.dent = NULL;
				nulref.top = NULL;
				return nulref;
			};
			
			diref.inode->mtime = time();
			vfsDirtyInode(diref.inode);
			
			dent = NEW(Dentry);
			memset(dent, 0, sizeof(Dentry));
			dent->name = strdup(entname);
			vfsUprefInode(diref.inode);
			dent->dir = diref.inode;
			dent->ino = 0;
			dent->key = __sync_fetch_and_add(&diref.inode->nextKey, 1);
			dent->flags = VFS_DENTRY_TEMP;
			
			if (diref.inode->dents == NULL)
			{
				diref.inode->dents = dent;
			}
			else
			{
				Dentry *last = diref.inode->dents;
				while (last->next != NULL) last = last->next;
				last->next = dent;
				dent->prev = last;
			};
			
			// the modificaiton and change times of the directory will be updated,
			// and marked dirty, once the caller does something with the dentry. so no
			// need to do it here.
			DentryRef dref;
			dref.dent = dent;
			dref.top = diref.top;
			return dref;
		}
		else
		{
			semSignal(&diref.inode->lock);
			vfsUnrefInode(diref);
			
			DentryRef nulref;
			nulref.dent = NULL;
			nulref.top = NULL;
			return nulref;
		};
	};
};

void vfsAppendDentry(Inode *dir, const char *name, ino_t ino)
{
	semWait(&dir->lock);
	
	Dentry *dent = NEW(Dentry);
	memset(dent, 0, sizeof(Dentry));
	dent->name = strdup(name);
	dent->dir = dir;
	vfsUprefInode(dir);
	dent->ino = ino;
	dent->key = __sync_fetch_and_add(&dir->nextKey, 1);
	
	if (dir->dents == NULL)
	{
		dir->dents = dent;
	}
	else
	{
		Dentry *last = dir->dents;
		while (last->next != NULL) last = last->next;
		last->next = dent;
		dent->prev = last;
	};
	
	semSignal(&dir->lock);
};

int vfsIsAllowed(Inode *inode, int perms)
{
	// if we have the XP_FSADMIN permission (or we are root), then we are allowed to perform
	// all operations.
	if (havePerm(XP_FSADMIN)) return 1;
	
	// since havePerm() always returns 1 if there are no process credentials, we can simply assume
	// at this point that we have them
	uid_t uid = getCurrentThread()->creds->euid;
	gid_t gid = getCurrentThread()->creds->egid;
	int numGroups = getCurrentThread()->creds->numGroups;
	gid_t groups[numGroups];
	memcpy(groups, getCurrentThread()->creds->groups, sizeof(gid_t) * numGroups);
	
	// check if the current user is listed in the ACL
	int i;
	for (i=0; i<VFS_ACL_SIZE; i++)
	{
		if (inode->acl[i].ace_type == VFS_ACE_USER && inode->acl[i].ace_id == uid)
		{
			return (inode->acl[i].ace_perms & perms) == perms;
		};
	};
	
	// find the union of permissions for all groups we are a part of
	int totalPerms = 0;
	int foundAny = 0;
	for (i=0; i<VFS_ACL_SIZE; i++)
	{
		if (inode->acl[i].ace_type == VFS_ACE_GROUP && inode->acl[i].ace_id == gid)
		{
			totalPerms |= inode->acl[i].ace_perms;
			foundAny = 1;
		};
		
		int j;
		for (j=0; j<numGroups; j++)
		{
			if (inode->acl[i].ace_type == VFS_ACE_GROUP && inode->acl[i].ace_id == groups[j])
			{
				totalPerms |= inode->acl[i].ace_perms;
				foundAny = 1;
			};
		};
	};
	
	// if none of our groups were found either, fall back to UNIX permissions
	if (!foundAny)
	{
		if (uid == inode->uid)
		{
			totalPerms = (inode->mode >> 6) & 7;
		}
		else if (gid == inode->gid)
		{
			totalPerms = (inode->mode >> 3) & 7;
		}
		else
		{
			int found = 0;
			for (i=0; i<numGroups; i++)
			{
				if (groups[i] == inode->gid)
				{
					totalPerms = (inode->mode >> 3) & 7;
					found = 1;
					break;
				};
			};
			
			if (!found) totalPerms = (inode->mode) & 7;
		};
	};
	
	// release the lock then just make sure we have the permissions
	return (totalPerms & perms) == perms;
};

char* vfsReadLink(Inode *link)
{
	semWait(&link->lock);
	char *target = NULL;
	if (link->target != NULL)
	{
		target = strdup(link->target);
	};
	semSignal(&link->lock);
	vfsDownrefInode(link);
	return target;
};

// this is used to implement vfsGetDentry(). it takes into account symbolic link depth
static DentryRef vfsGetDentryRecur(InodeRef startdir, const char *path, int create, int *error, int depth)
{
	static DentryRef nulref = {NULL, NULL};
	
	// check if we exceeded the depth
	if (depth == VFS_MAX_LINK_DEPTH)
	{
		vfsUnrefInode(startdir);
		if (error != NULL) *error = ELOOP;
		return nulref;
	};
	
	// empty paths must not resolve successfully
	if (path[0] == 0)
	{
		if (error != NULL) *error = ENOENT;
		vfsUnrefInode(startdir);
		return nulref;
	};
	
	// decide starting directory
	if (path[0] == '/')
	{
		vfsUnrefInode(startdir);
		startdir = vfsGetRoot();
		path++;
	}
	else if (startdir.inode == NULL)
	{
		vfsUnrefInode(startdir);
		startdir = vfsGetCurrentDir();
	};
	
	// start with the dentry corresponding to our start directory
	DentryRef currentDent = vfsGetParentDentry(startdir);
	
	// iterate down the tree
	char *pathbuf = strdup(path);
	char *scan = pathbuf;
	
	int isFinal = 0;
	while (!isFinal)
	{
		char *token = scan;
		char *slashPos = strstr(scan, "/");
		if (slashPos == NULL)
		{
			isFinal = 1;
		}
		else
		{
			*slashPos = 0;
			scan = slashPos + 1;
		};
		
		// get the current directory inode (following links if necessary)
		InodeRef dir = vfsGetInode(currentDent, 1, error);
		if (dir.inode == NULL)
		{
			vfsUnrefInode(dir);
			kfree(pathbuf);
			return nulref;
		};
		
		// the inode must be a directory
		if ((dir.inode->mode & 0xF000) != VFS_MODE_DIRECTORY)
		{
			vfsUnrefInode(dir);
			kfree(pathbuf);
			if (error != NULL) *error = ENOTDIR;
			return nulref;
		};
		
		// check if we have the required permissions
		int neededPerms = VFS_ACE_EXEC;
		if (create) neededPerms |= VFS_ACE_WRITE;
		semWait(&dir.inode->lock);
		if (!vfsIsAllowed(dir.inode, neededPerms))
		{
			semSignal(&dir.inode->lock);
			vfsUnrefInode(dir);
			kfree(pathbuf);
			if (error != NULL) *error = EACCES;
			return nulref;
		};
		semSignal(&dir.inode->lock);
		
		// all good
		currentDent = vfsGetChildDentry(dir, token, (create) && (isFinal));
		if (currentDent.dent == NULL)
		{
			vfsUnrefDentry(currentDent);
			kfree(pathbuf);
			if (error != NULL) *error = ENOENT;
			return nulref;
		};
	};
	
	kfree(pathbuf);
	return currentDent;
};

DentryRef vfsGetDentry(InodeRef startdir, const char *path, int create, int *error)
{
	return vfsGetDentryRecur(startdir, path, create, error, 0);
};

void vfsUnrefDentry(DentryRef dref)
{
	if (dref.dent != NULL)
	{
		Inode *dir = dref.dent->dir;
		semSignal(&dir->lock);
		vfsDownrefInode(dir);
	};
	
	while (dref.top != NULL)
	{
		MountPoint *mnt = dref.top;
		dref.top = mnt->prev;
		
		vfsDownrefInode(mnt->root);
		vfsDownrefInode(mnt->dent->dir);
		kfree(mnt);
	};
};

void vfsUnrefInode(InodeRef iref)
{
	if (iref.inode != NULL) vfsDownrefInode(iref.inode);

	while (iref.top != NULL)
	{
		MountPoint *mnt = iref.top;
		iref.top = mnt->prev;
		
		vfsDownrefInode(mnt->root);
		vfsDownrefInode(mnt->dent->dir);
		kfree(mnt);
	};
};

void vfsLinkInode(DentryRef dref, Inode *target)
{
	assert(dref.dent->ino == 0);
	vfsUprefInode(target);
	dref.dent->ino = target->ino;
	dref.dent->target = target;
	dref.dent->flags &= ~VFS_DENTRY_TEMP;
	if (target->parent == NULL)
	{
		target->parent = dref.dent;
	};
	
	vfsDirtyInode(dref.dent->dir);
	vfsUnrefDentry(dref);
	
	semWait(&target->lock);
	target->links++;
	__sync_fetch_and_add(&target->dups, 1);
	vfsDirtyInode(target);
	semSignal(&target->lock);
};

InodeRef vfsLinkAndGetInode(DentryRef dref, Inode *target)
{
	assert(dref.dent->ino == 0);
	vfsUprefInode(target);
	dref.dent->ino = target->ino;
	dref.dent->target = target;
	dref.dent->flags &= ~VFS_DENTRY_TEMP;
	if (target->parent == NULL)
	{
		target->parent = dref.dent;
	};
	
	vfsDirtyInode(dref.dent->dir);
	
	semWait(&target->lock);
	target->links++;
	__sync_fetch_and_add(&target->dups, 1);
	vfsDirtyInode(target);
	semSignal(&target->lock);
	
	return vfsGetDentryTarget(dref);
};

void vfsBindInode(DentryRef dref, Inode *target)
{
	assert(dref.dent->ino == 0);
	vfsUprefInode(target);
	dref.dent->ino = target->ino;
	dref.dent->target = target;
	dref.dent->flags |= VFS_DENTRY_TEMP;
	if (target->parent == NULL)
	{
		target->parent = dref.dent;
		vfsUprefInode(dref.dent->dir);
	};
	
	vfsDirtyInode(dref.dent->dir);
	vfsUnrefDentry(dref);
	
	semWait(&target->lock);
	target->links++;
	vfsDirtyInode(target);
	semSignal(&target->lock);
};

static void vfsLinkDown(Inode *inode)
{
	semWait(&inode->lock);
	if ((--inode->links) == 0)
	{	
		if (inode->drop != NULL)
		{
			inode->drop(inode);
		};
		
		if (inode->ft != NULL)
		{
			ftUp(inode->ft);
			ftUncache(inode->ft);
			ftDown(inode->ft);
			inode->ft = NULL;
		};

		inode->ino = 0;
	};
	
	__sync_fetch_and_add(&inode->dups, -1);
	semSignal(&inode->lock);
	vfsDownrefInode(inode);
};

int vfsUnlinkInode(DentryRef dref, int flags)
{
	if ((dref.dent->dir->fs->flags & VFS_ST_RDONLY) && ((flags & VFS_AT_NOVALID) == 0))
	{
		vfsUnrefDentry(dref);
		return EROFS;
	};
	
	if (dref.dent == kernelRootDentry || (dref.dent->flags & VFS_DENTRY_MNTPOINT))
	{
		vfsUnrefDentry(dref);
		return EBUSY;
	};
	
	vfsCallInInode(dref.dent);
	if (dref.dent->target == NULL)
	{
		vfsUnrefDentry(dref);
		return EIO;
	};
	
	if (dref.dent->target->flags & VFS_INODE_NOUNLINK)
	{
		vfsUnrefDentry(dref);
		return EPERM;
	};
	
	if (flags & VFS_AT_REMOVEDIR)
	{
		if ((dref.dent->target->mode & 0xF000) != VFS_MODE_DIRECTORY)
		{
			vfsUnrefDentry(dref);
			return ENOTDIR;
		};
		
		if (dref.dent->target->dents != NULL)
		{
			vfsUnrefDentry(dref);
			return ENOTEMPTY;
		};
		
		if (dref.dent->target->refcount != 1)
		{
			vfsUnrefDentry(dref);
			return EBUSY;
		};
	}
	else
	{
		// do not delete directories in this case
		if ((dref.dent->target->mode & 0xF000) == VFS_MODE_DIRECTORY)
		{
			vfsUnrefDentry(dref);
			return EISDIR;
		};
	};
	
	// if we do not have the XP_FSADMIN permission, then make sure we are allowed to
	// unlink this entry
	if (!havePerm(XP_FSADMIN))
	{
		if (dref.dent->dir->mode & VFS_MODE_STICKY)
		{
			// if the parent directory has the sticky bit set, then the owner of either
			// the parent directory or the entry itself may delete it
			uid_t uid = 0;
			if (getCurrentThread() != NULL && getCurrentThread()->creds != NULL) uid = getCurrentThread()->creds->euid;
			
			if (dref.dent->dir->uid != uid && dref.dent->target->uid != uid)
			{
				vfsUnrefDentry(dref);
				return EACCES;
			};
		}
		else if (!vfsIsAllowed(dref.dent->dir, VFS_ACE_WRITE))
		{
			// in other cases, we must have write permission on the parent directory
			vfsUnrefDentry(dref);
			return EACCES;
		};
	};
	
	dref.dent->dir->atime = dref.dent->dir->ctime = dref.dent->dir->mtime = time();
	
	Inode *inode = dref.dent->target;
	dref.dent->target = NULL;
	dref.dent->ino = 0;
	vfsRemoveDentry(dref);
	
	vfsLinkDown(inode);
	return 0;
};

void vfsRemoveDentry(DentryRef dref)
{
	assert(dref.dent->ino == 0);
	
	if (dref.dent->prev != NULL) dref.dent->prev->next = dref.dent->next;
	if (dref.dent->next != NULL) dref.dent->next->prev = dref.dent->prev;
	if (dref.dent->dir->dents == dref.dent) dref.dent->dir->dents = dref.dent->next;
	
	Inode *dir = dref.dent->dir;
	vfsDirtyInode(dir);
	semSignal(&dir->lock);
	vfsDownrefInode(dir);
	vfsDownrefInode(dir);

	while (dref.top != NULL)
	{
		MountPoint *mnt = dref.top;
		dref.top = mnt->prev;
		
		vfsDownrefInode(mnt->root);
		vfsDownrefInode(mnt->dent->dir);
		kfree(mnt);
	};

	kfree(dref.dent->name);
	kfree(dref.dent);
};

int vfsMakeDir(InodeRef startdir, const char *path, mode_t mode)
{
	return vfsMakeDirEx(startdir, path, mode, 0);
};

int vfsMakeDirEx(InodeRef startdir, const char *path, mode_t mode, int flags)
{
	int error;
	DentryRef dref = vfsGetDentry(startdir, path, 1, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		return error;
	};
	
	if (dref.dent->ino != 0)
	{
		vfsUnrefDentry(dref);
		return EEXIST;
	};
	
	if ((dref.dent->dir->fs->flags & VFS_ST_RDONLY) && ((flags & VFS_MKDIR_NOVALID) == 0))
	{
		vfsRemoveDentry(dref);
		return EROFS;
	};
	
	Inode *newdir = vfsCreateInode(dref.dent->dir->fs, VFS_MODE_DIRECTORY | (mode & 0x0FFF));
	if (newdir == NULL)
	{
		vfsRemoveDentry(dref);
		return ERRNO;
	};
	
	vfsLinkInode(dref, newdir);
	vfsDownrefInode(newdir);
	return 0;
};

int vfsMount(DentryRef dref, Inode *mntroot, int flags)
{
	if (mntroot->fs != NULL)
	{
		if (mntroot->fs->numMounts == 0)
		{
			// new filesystem; apply flags
			if (flags & MNT_RDONLY) mntroot->fs->flags |= VFS_ST_RDONLY;
			if (flags & MNT_NOSUID) mntroot->fs->flags |= VFS_ST_NOSUID;
		};
	};
	
	if (!havePerm(XP_MOUNT))
	{
		vfsUnrefDentry(dref);
		return EACCES;
	};
	
	if (dref.dent->flags & VFS_DENTRY_MNTPOINT)
	{
		// already a mountpoint
		vfsUnrefDentry(dref);
		return EBUSY;
	};
	
	if (dref.dent->target != NULL)
	{
		if (dref.dent->target->refcount != 1)
		{
			// mountpoints can only be made on unused dentries (empty directories or
			// closed files)
			vfsUnrefDentry(dref);
			return EBUSY;
		};
	};
	
	Inode *oldTarget = dref.dent->target;
	dref.dent->target = mntroot;
	dref.dent->flags |= VFS_DENTRY_MNTPOINT;
	vfsUprefInode(mntroot);
	__sync_fetch_and_add(&mntroot->fs->numMounts, 1);
	
	// increment the reference count of the inode holding the dentry. this will make it impossible
	// to unmount a filesystem if there are other mountpoints under it (see vfsUnmount() ).
	vfsUprefInode(dref.dent->dir);
	
	vfsUnrefDentry(dref);
	if (oldTarget != NULL) vfsDownrefInode(oldTarget);
	return 0;
};

File* vfsOpen(InodeRef startdir, const char *path, int oflag, mode_t mode, int *error)
{
	DentryRef dref = vfsGetDentry(startdir, path, oflag & O_CREAT, error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		return NULL;
	};
	
	InodeRef iref;
	if (dref.dent->ino != 0)
	{
		if (oflag & O_EXCL)
		{
			vfsUnrefDentry(dref);
			if (error != NULL) *error = EEXIST;
			return NULL;
		};
		
		if (dref.dent->dir->fs->flags & VFS_ST_RDONLY)
		{
			if (oflag & O_WRONLY)
			{
				vfsUnrefDentry(dref);
				if (error != NULL) *error = EROFS;
				return NULL;
			};
		};
		
		iref = vfsGetInode(dref, 1, error);
	}
	else
	{
		if (dref.dent->dir->fs->flags & VFS_ST_RDONLY)
		{
			vfsRemoveDentry(dref);
			if (error != NULL) *error = EROFS;
			return NULL;
		};
		
		// create the inode
		Inode *newInode = vfsCreateInode(dref.dent->dir->fs, mode & 0x0FFF);
		if (newInode == NULL)
		{
			if (error != NULL) *error = ERRNO;
			vfsRemoveDentry(dref);
			return NULL;
		};
		
		iref = vfsLinkAndGetInode(dref, newInode);
		vfsDownrefInode(newInode);
	};
	
	// get the inode
	if (iref.inode == NULL)
	{
		vfsUnrefInode(iref);
		return NULL;
	};
	
	// check permissions
	int permsNeeded = 0;
	if (oflag & O_RDONLY) permsNeeded |= VFS_ACE_READ;
	if (oflag & O_WRONLY) permsNeeded |= VFS_ACE_WRITE;
	
	semWait(&iref.inode->lock);
	if (!vfsIsAllowed(iref.inode, permsNeeded))
	{
		semSignal(&iref.inode->lock);
		vfsUnrefInode(iref);
		if (error != NULL) *error = EACCES;
		return NULL;
	};
	semSignal(&iref.inode->lock);
	
	// create the description
	return vfsOpenInode(iref, oflag, error);
};

File* vfsOpenInode(InodeRef iref, int oflag, int *error)
{
	__sync_fetch_and_add(&iref.inode->numOpens, 1);
	iref.inode->atime = time();
		
	void *filedata = NULL;
	if (iref.inode->open != NULL)
	{
		filedata = iref.inode->open(iref.inode, oflag);
		if (filedata == NULL)
		{
			if (error != NULL) *error = ERRNO;
			vfsUnrefInode(iref);
			return NULL;
		};
	};

	File *fp = NEW(File);
	memset(fp, 0, sizeof(File));
	
	fp->iref = iref;
	semInit(&fp->lock);
	fp->filedata = filedata;
	fp->offset = 0;
	fp->oflags = oflag;
	fp->refcount = 1;
	
	if (oflag & O_TRUNC)
	{
		if (iref.inode->ft != NULL)
		{
			ftTruncate(iref.inode->ft, 0);
			iref.inode->mtime = iref.inode->ctime = time();
		};
	};

	if (iref.inode->ft != NULL) ftUp(iref.inode->ft);
	vfsDirtyInode(fp->iref.inode);
	return fp;
};

void vfsDup(File *fp)
{
	__sync_fetch_and_add(&fp->refcount, 1);
};

void vfsClose(File *fp)
{	
	if (__sync_add_and_fetch(&fp->refcount, -1) == 0)
	{
		if (fp->iref.inode->ft != NULL) ftReleaseProcessLocks(fp->iref.inode->ft);	
		
		if (fp->iref.inode->close != NULL)
		{
			fp->iref.inode->close(fp->iref.inode, fp->filedata);
		};
		
		if (fp->iref.inode->ft != NULL) ftDown(fp->iref.inode->ft);
		__sync_fetch_and_add(&fp->iref.inode->numOpens, -1);
		vfsUnrefInode(fp->iref);
		kfree(fp);
	};
};

static ssize_t vfsReadUnlocked(File *fp, void *buffer, size_t size, off_t offset)
{
	if (offset < 0)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if ((~size) < (size_t) offset)
	{
		ERRNO = EOVERFLOW;
		return -1;
	};
	
	if (fp->iref.inode->pread != NULL)
	{
		return fp->iref.inode->pread(fp->iref.inode, fp, buffer, size, offset);
	}
	else if (fp->iref.inode->ft != NULL)
	{
		return ftRead(fp->iref.inode->ft, buffer, size, offset);
	};
	
	fp->iref.inode->atime = time();
	
	ERRNO = EPERM;
	return -1;
};

static ssize_t vfsWriteUnlocked(File *fp, const void *buffer, size_t size, off_t offset)
{
	if (offset < 0)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if ((~size) < (size_t) offset)
	{
		ERRNO = EOVERFLOW;
		return -1;
	};
	
	if (fp->iref.inode->pwrite != NULL)
	{
		return fp->iref.inode->pwrite(fp->iref.inode, fp, buffer, size, offset);
	}
	else if (fp->iref.inode->ft != NULL)
	{
		return ftWrite(fp->iref.inode->ft, buffer, size, offset);
	};
	
	fp->iref.inode->mtime = fp->iref.inode->ctime = time();

	ERRNO = EPERM;
	return -1;
};

ssize_t vfsRead(File *fp, void *buffer, size_t size)
{
	if ((fp->oflags & O_RDONLY) == 0)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	semWait(&fp->lock);
	ssize_t sz = vfsReadUnlocked(fp, buffer, size, fp->offset);
	if (sz != -1) fp->offset += sz;
	semSignal(&fp->lock);
	return sz;
};

ssize_t vfsPRead(File *fp, void *buffer, size_t size, off_t offset)
{
	if ((fp->oflags & O_RDONLY) == 0)
	{
		ERRNO = EBADF;
		return -1;
	};

	semWait(&fp->lock);
	ssize_t sz = vfsReadUnlocked(fp, buffer, size, offset);
	semSignal(&fp->lock);
	return sz;
};

ssize_t vfsWrite(File *fp, const void *buffer, size_t size)
{
	if ((fp->oflags & O_WRONLY) == 0)
	{
		ERRNO = EBADF;
		return -1;
	};

	semWait(&fp->lock);
	off_t offset = fp->offset;
	if (fp->oflags & O_APPEND)
	{
		if (fp->iref.inode->ft != NULL)
		{
			offset = (off_t) fp->iref.inode->ft->size;
		};
	};
	
	ssize_t sz = vfsWriteUnlocked(fp, buffer, size, offset);
	if (sz != -1 && ((fp->oflags & O_APPEND) == 0)) fp->offset += sz;
	
	semSignal(&fp->lock);
	return sz;
};

ssize_t vfsPWrite(File *fp, const void *buffer, size_t size, off_t offset)
{
	semWait(&fp->lock);
	ssize_t sz = vfsWriteUnlocked(fp, buffer, size, offset);
	semSignal(&fp->lock);
	return sz;
};

off_t vfsSeek(File *fp, off_t off, int whence)
{
	size_t size;
	if (fp->iref.inode->ft != NULL)
	{
		size = fp->iref.inode->ft->size;
	}
	else if (fp->iref.inode->getsize != NULL)
	{
		semWait(&fp->iref.inode->lock);
		size = fp->iref.inode->getsize(fp->iref.inode);
		semSignal(&fp->iref.inode->lock);
	}
	else
	{
		ERRNO = ESPIPE;
		return (off_t) -1;
	};
	
	semWait(&fp->lock);
	
	off_t newOffset;
	switch (whence)
	{
	case VFS_SEEK_CUR:
		newOffset = fp->offset + off;
		break;
	case VFS_SEEK_END:
		newOffset = size + off;
		break;
	case VFS_SEEK_SET:
		newOffset = off;
		break;
	default:
		semSignal(&fp->lock);
		ERRNO = EINVAL;
		return (off_t) -1;
	};
	
	if (newOffset < 0)
	{
		semSignal(&fp->lock);
		ERRNO = EINVAL;
		return (off_t) -1;
	};
	
	fp->offset = newOffset;
	semSignal(&fp->lock);
	return newOffset;
};

void vfsInodeStat(Inode *inode, struct kstat *st)
{
	semWait(&inode->lock);
	if (inode->fs == NULL)
	{
		st->st_dev = 0;
	}
	else
	{
		st->st_dev = inode->fs->fsid;
	};
	
	st->st_ino = inode->ino;
	st->st_mode = inode->mode;
	st->st_nlink = inode->links + inode->numOpens;
	st->st_uid = inode->uid;
	st->st_gid = inode->gid;
	st->st_rdev = 0;
	
	if (inode->getsize != NULL)
	{
		st->st_size = inode->getsize(inode);
	}
	else if (inode->ft == NULL)
	{
		st->st_size = 0;
	}
	else
	{
		st->st_size = inode->ft->size;
	};
	
	if (inode->fs != NULL) st->st_blksize = inode->fs->blockSize;
	else st->st_blksize = 0;
	
	st->st_blocks = inode->numBlocks;
	st->st_atime = inode->atime;
	st->st_mtime = inode->mtime;
	st->st_ctime = inode->ctime;
	st->st_ixperm = inode->ixperm;
	st->st_oxperm = inode->oxperm;
	st->st_dxperm = inode->dxperm;
	st->st_btime = inode->btime;
	memcpy(st->st_acl, inode->acl, sizeof(AccessControlEntry) * VFS_ACL_SIZE);
	semSignal(&inode->lock);
};

void vfsInodeStatVFS(Inode *inode, struct kstatvfs *st)
{
	memset(st, 0, sizeof(struct kstatvfs));
	if (inode->fs == NULL) return;
	
	st->f_bsize = inode->fs->blockSize;
	st->f_frsize = inode->fs->blockSize;
	st->f_blocks = inode->fs->blocks;
	st->f_bfree = inode->fs->freeBlocks;
	st->f_bavail = st->f_bfree;
	st->f_files = inode->fs->inodes;
	st->f_ffree = inode->fs->freeInodes;
	st->f_favail = st->f_ffree;
	st->f_fsid = inode->fs->fsid;
	st->f_flag = inode->fs->flags;
	st->f_namemax = inode->fs->maxnamelen;
	strcpy(st->f_fstype, inode->fs->fstype);
	memcpy(st->f_bootid, inode->fs->bootid, 16);
};

int vfsStat(InodeRef startdir, const char *path, int follow, struct kstat *st)
{
	int error;
	DentryRef dref = vfsGetDentry(startdir, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		ERRNO = error;
		return -1;
	};
	
	InodeRef iref = vfsGetInode(dref, follow, &error);
	if (iref.inode == NULL)
	{
		ERRNO = error;
		return -1;
	};
	
	vfsInodeStat(iref.inode, st);
	vfsUnrefInode(iref);
	return 0;
};

int vfsStatVFS(InodeRef startdir, const char *path, struct kstatvfs *st)
{
	int error;
	DentryRef dref = vfsGetDentry(startdir, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		ERRNO = error;
		return -1;
	};
	
	InodeRef iref = vfsGetInode(dref, 0, &error);
	if (iref.inode == NULL)
	{
		ERRNO = error;
		return -1;
	};
	
	vfsInodeStatVFS(iref.inode, st);
	vfsUnrefInode(iref);
	return 0;
};

int vfsInodeChangeMode(Inode *inode, mode_t mode)
{	
	uid_t myUID;
	if (getCurrentThread() == NULL || getCurrentThread()->creds == NULL)
	{
		myUID = 0;
	}
	else
	{
		myUID = getCurrentThread()->creds->euid;
	};

	semWait(&inode->lock);	
	if (inode->fs->flags & VFS_ST_RDONLY)
	{
		semSignal(&inode->lock);
		ERRNO = EROFS;
		return -1;
	};
	if ((inode->uid != myUID) && !havePerm(XP_FSADMIN))
	{
		semSignal(&inode->lock);
		ERRNO = EACCES;
		return -1;
	};
	
	inode->mode = (inode->mode & 0xF000) | (mode & 0x0FFF);
	vfsDirtyInode(inode);
	semSignal(&inode->lock);
	return 0;
};

int vfsChangeMode(InodeRef startdir, const char *path, mode_t mode)
{
	int error;
	DentryRef dref = vfsGetDentry(startdir, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		ERRNO = error;
		return -1;
	};
	
	InodeRef iref = vfsGetInode(dref, 1, &error);
	if (iref.inode == NULL)
	{
		ERRNO = error;
		return -1;
	};
	
	int ret = vfsInodeChangeMode(iref.inode, mode);
	error = ERRNO;
	vfsUnrefInode(iref);
	ERRNO = error;
	return ret;
};

int vfsInodeChangeOwner(Inode *inode, uid_t uid, gid_t gid)
{
	if (uid != -1)
	{
		if ((uid & 0xFFFF) != uid)
		{
			ERRNO = EINVAL;
			return -1;
		};
	};
	
	if (gid != -1)
	{
		if ((gid & 0xFFFF) != gid)
		{
			ERRNO = EINVAL;
			return -1;
		};
	};
	
	semWait(&inode->lock);
	if (inode->fs->flags & VFS_ST_RDONLY)
	{
		semSignal(&inode->lock);
		ERRNO = EROFS;
		return -1;
	};

	uid_t myUID;
	if (getCurrentThread() == NULL || getCurrentThread()->creds == NULL)
	{
		myUID = 0;
	}
	else
	{
		myUID = getCurrentThread()->creds->euid;
	};
	
	// check permissions
	if (!havePerm(XP_FSADMIN))
	{
		if (uid != -1 || gid != -1)
		{
			if (uid != -1 && uid != inode->uid)
			{
				semSignal(&inode->lock);
				ERRNO = EACCES;
				return -1;
			};
			
			if (gid != -1 && gid != inode->gid && myUID != inode->uid)
			{
				semSignal(&inode->lock);
				ERRNO = EACCES;
				return -1;
			};
		};
	};
	
	// finally, change the owner/group
	if (uid != -1) inode->uid = uid;
	if (gid != -1) inode->gid = gid;
	vfsDirtyInode(inode);
	semSignal(&inode->lock);
	return 0;
};

int vfsChangeOwner(InodeRef startdir, const char *path, uid_t uid, gid_t gid)
{
	int error;
	DentryRef dref = vfsGetDentry(startdir, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		ERRNO = error;
		return -1;
	};
	
	InodeRef iref = vfsGetInode(dref, 1, &error);
	if (iref.inode == NULL)
	{
		ERRNO = error;
		return -1;
	};
	
	int ret = vfsInodeChangeOwner(iref.inode, uid, gid);
	error = ERRNO;
	vfsUnrefInode(iref);
	ERRNO = error;
	return ret;
};

int vfsTruncate(Inode *inode, off_t size)
{
	if (inode->fs->flags & VFS_ST_RDONLY)
	{
		semSignal(&inode->lock);
		ERRNO = EROFS;
		return -1;
	};

	if (size < -1)
	{
		return EINVAL;
	};
	
	if (inode->ft == NULL)
	{
		return EINVAL;
	};
	
	ftTruncate(inode->ft, (size_t) size);
	inode->mtime = inode->ctime = time();
	
	return 0;
};

InodeRef vfsCopyInodeRef(InodeRef old)
{
	InodeRef new;
	new.inode = old.inode;
	new.top = NULL;
	vfsUprefInode(new.inode);
	
	MountPoint *mnt;
	MountPoint *bottom = NULL;
	for (mnt=old.top; mnt!=NULL; mnt=mnt->prev)
	{
		MountPoint *copy = NEW(MountPoint);
		copy->dent = mnt->dent;
		vfsUprefInode(copy->dent->dir);
		copy->root = mnt->root;
		vfsUprefInode(copy->root);
		copy->prev = NULL;
		
		if (bottom == NULL)
		{
			bottom = new.top = copy;
		}
		else
		{
			bottom->prev = copy;
			bottom = copy;
		};
	};
	
	return new;
};

int vfsChangeDir(InodeRef startdir, const char *path)
{
	Creds *creds = getCurrentThread()->creds;
	if (creds == NULL)
	{
		vfsUnrefInode(startdir);
		return EPERM;
	};
	
	int error;
	DentryRef dref = vfsGetDentry(startdir, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		return error;
	};
	
	InodeRef iref = vfsGetInode(dref, 1, &error);
	if (iref.inode == NULL)
	{
		vfsUnrefInode(iref);
		return error;
	};
	
	if ((iref.inode->mode & VFS_MODE_TYPEMASK) != VFS_MODE_DIRECTORY)
	{
		vfsUnrefInode(iref);
		return ENOTDIR;
	};
	
	semWait(&iref.inode->lock);
	if (!vfsIsAllowed(iref.inode, VFS_ACE_EXEC))
	{
		semSignal(&iref.inode->lock);
		vfsUnrefInode(iref);
		return EACCES;
	};
	semSignal(&iref.inode->lock);
	
	semWait(&creds->semDir);
	InodeRef old = creds->cwd;
	creds->cwd = iref;
	semSignal(&creds->semDir);
	
	vfsUnrefInode(old);
	return 0;
};

int vfsChangeRoot(InodeRef startdir, const char *path)
{
	Creds *creds = getCurrentThread()->creds;
	if (creds == NULL)
	{
		vfsUnrefInode(startdir);
		return EPERM;
	};
	
	int error;
	DentryRef dref = vfsGetDentry(startdir, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		return error;
	};
	
	InodeRef iref = vfsGetInode(dref, 1, &error);
	if (iref.inode == NULL)
	{
		vfsUnrefInode(iref);
		return error;
	};
	
	if ((iref.inode->mode & VFS_MODE_TYPEMASK) != VFS_MODE_DIRECTORY)
	{
		vfsUnrefInode(iref);
		return ENOTDIR;
	};
	
	semWait(&iref.inode->lock);
	if (!vfsIsAllowed(iref.inode, VFS_ACE_EXEC))
	{
		semSignal(&iref.inode->lock);
		vfsUnrefInode(iref);
		return EACCES;
	};
	semSignal(&iref.inode->lock);
	
	semWait(&creds->semDir);
	InodeRef old = creds->rootdir;
	creds->rootdir = iref;
	semSignal(&creds->semDir);
	
	vfsUnrefInode(old);
	return 0;
};

typedef struct CompChain_
{
	char*			comp;
	struct CompChain_*	next;
} CompChain;

static char* vfsRealPathRecur(InodeRef rootdir, CompChain *next, DentryRef dref)
{
	CompChain frame;
	frame.comp = strdup(dref.dent->name);
	frame.next = next;
	
	InodeRef parent = vfsGetDentryContainer(dref);
	if (parent.inode == rootdir.inode)
	{
		// we reached the root directory and so we can now concatenate the
		// components into a path.
		size_t totalSize = 1;	// NUL byte
		
		CompChain *scan;
		for (scan=&frame; scan!=NULL; scan=scan->next)
		{
			totalSize += (1 + strlen(scan->comp));
		};
		
		char *result = (char*) kmalloc(totalSize);
		char *put = result;
		for (scan=&frame; scan!=NULL; scan=scan->next)
		{
			*put++ = '/';
			strcpy(put, scan->comp);
			put += strlen(scan->comp);
		};
		
		*put = 0;
		kfree(frame.comp);
		return result;	
	};
	
	if (parent.inode == kernelRootInode)
	{
		// we reached the kernel root without reaching the process root, meaning that the
		// given dentry is unreachable.
		kfree(frame.comp);
		return strdup("(unreachable)");
	};
	
	DentryRef updref = vfsGetParentDentry(parent);
	char *result = vfsRealPathRecur(rootdir, &frame, updref);
	kfree(frame.comp);
	return result;
};

char* vfsRealPath(DentryRef dref)
{
	InodeRef rootdir = vfsGetRoot();
	if (rootdir.inode == dref.dent->target)
	{
		vfsUnrefDentry(dref);
		vfsUnrefInode(rootdir);
		return strdup("/");
	};
	
	char *result = vfsRealPathRecur(rootdir, NULL, dref);
	vfsUnrefInode(rootdir);
	return result;
};

char* vfsGetCurrentDirPath()
{
	InodeRef cwd = vfsGetCurrentDir();
	DentryRef dref = vfsGetParentDentry(cwd);
	return vfsRealPath(dref);
};

int vfsCreateLink(InodeRef oldstart, const char *oldpath, InodeRef newstart, const char *newpath, int flags)
{
	int allFlags = VFS_AT_SYMLINK_FOLLOW;
	if ((flags & ~allFlags) != 0)
	{
		vfsUnrefInode(oldstart);
		vfsUnrefInode(newstart);
		return EINVAL;
	};
	
	int error;
	DentryRef drefOld = vfsGetDentry(oldstart, oldpath, 0, &error);
	if (drefOld.dent == NULL)
	{
		vfsUnrefDentry(drefOld);
		vfsUnrefInode(newstart);
		return error;
	};
	
	int temp = drefOld.dent->flags & VFS_DENTRY_TEMP;
	
	InodeRef iref = vfsGetInode(drefOld, flags & VFS_AT_SYMLINK_FOLLOW, &error);
	if (iref.inode == NULL)
	{
		vfsUnrefInode(newstart);
		return error;
	};
	
	if ((iref.inode->mode & VFS_MODE_TYPEMASK) == VFS_MODE_DIRECTORY)
	{
		vfsUnrefInode(newstart);
		vfsUnrefInode(iref);
		return EPERM;
	};
	
	DentryRef drefNew = vfsGetDentry(newstart, newpath, 1, &error);
	if (drefNew.dent == NULL)
	{
		vfsUnrefDentry(drefNew);
		vfsUnrefInode(iref);
		return error;
	};
	
	if (drefNew.dent->ino != 0)
	{
		vfsUnrefInode(iref);
		vfsUnrefDentry(drefNew);
		return EEXIST;
	};
	
	if ((drefNew.dent->dir->fs->flags & VFS_ST_RDONLY) && ((flags & VFS_AT_NOVALID) == 0))
	{
		vfsUnrefInode(iref);
		vfsRemoveDentry(drefNew);
		return EROFS;
	};
	
	if (drefNew.dent->dir->fs != iref.inode->fs)
	{
		// cross-device link attempted
		vfsRemoveDentry(drefNew);
		vfsUnrefInode(iref);
		return EXDEV;
	};
	
	if (temp) vfsBindInode(drefNew, iref.inode);
	else vfsLinkInode(drefNew, iref.inode);
	vfsUnrefInode(iref);
	return 0;
};

int vfsCreateSymlink(const char *oldpath, InodeRef newstart, const char *newpath)
{
	return vfsCreateSymlinkEx(oldpath, newstart, newpath, 0);
};

int vfsCreateSymlinkEx(const char *oldpath, InodeRef newstart, const char *newpath, int flags)
{
	int error;
	DentryRef dref = vfsGetDentry(newstart, newpath, 1, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		return error;
	};
	
	if (dref.dent->ino != 0)
	{
		vfsUnrefDentry(dref);
		return EEXIST;
	};
	
	if ((dref.dent->dir->fs->flags & VFS_ST_RDONLY) && ((flags & VFS_AT_NOVALID) == 0))
	{
		vfsRemoveDentry(dref);
		return EROFS;
	};
	
	Inode *link = vfsCreateInode(dref.dent->dir->fs, VFS_MODE_LINK | 0777);
	if (link == NULL)
	{
		vfsRemoveDentry(dref);
		return ERRNO;
	};
	
	link->target = strdup(oldpath);
	
	vfsLinkInode(dref, link);
	vfsDownrefInode(link);
	return 0;
};

char* vfsReadLinkPath(InodeRef startdir, const char *path)
{
	int error;
	DentryRef dref = vfsGetDentry(startdir, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		ERRNO = error;
		return NULL;
	};
	
	InodeRef iref = vfsGetInode(dref, 0, &error);
	if (iref.inode == NULL)
	{
		ERRNO = error;
		return NULL;
	};
	
	if (iref.inode->target == NULL)
	{
		vfsUnrefInode(iref);
		ERRNO = EINVAL;
		return NULL;
	};
	
	char *result = strdup(iref.inode->target);
	vfsUnrefInode(iref);
	return result;
};

int vfsInodeChangeTimes(Inode *inode, time_t atime, uint32_t anano, time_t mtime, uint32_t mnano)
{
	semWait(&inode->lock);

	uid_t myUID;
	if (getCurrentThread()->creds == NULL)
	{
		myUID = 0;
	}
	else
	{
		myUID = getCurrentThread()->creds->euid;
	};

	if (inode->uid == myUID || havePerm(XP_FSADMIN))
	{
		// the owner, or filesystem admin, can set any times they want
		if (atime == 0)
		{
			atime = time();
			anano = 0;
		};
		
		if (mtime == 0)
		{
			mtime = time();
			mnano = 0;
		};
		
		inode->atime = atime;
		inode->anano = anano;
		inode->mtime = mtime;
		inode->mnano = mnano;
		inode->ctime = time();
		inode->cnano = 0;
	}
	else if (vfsIsAllowed(inode, VFS_ACE_WRITE) && atime == 0 && mtime == 0)
	{
		inode->anano = inode->mnano = inode->cnano = 0;
		inode->atime = inode->mtime = inode->ctime = time();
	}
	else
	{
		semSignal(&inode->lock);
		if (atime == 0 && mtime == 0) ERRNO = EACCES;
		else ERRNO = EPERM;
		return -1;
	};
	
	vfsDirtyInode(inode);
	semSignal(&inode->lock);
	return 0;
};

int vfsChangeTimes(InodeRef startdir, const char *path, time_t atime, uint32_t anano, time_t mtime, uint32_t mnano)
{
	int error;
	DentryRef dref = vfsGetDentry(startdir, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		ERRNO = error;
		return -1;
	};
	
	InodeRef iref = vfsGetInode(dref, 1, &error);
	if (iref.inode == NULL)
	{
		ERRNO = error;
		return -1;
	};
	
	int ret = vfsInodeChangeTimes(iref.inode, atime, anano, mtime, mnano);
	error = ERRNO;
	vfsUnrefInode(iref);
	ERRNO = error;
	return ret;
};

int vfsUnmount(const char *path, int flags)
{
	if (flags != 0)
	{
		return EINVAL;
	};
	
	if (!havePerm(XP_MOUNT))
	{
		return EACCES;
	};
	
	int error;
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		return error;
	};
	
	if (dref.dent == kernelRootDentry)
	{
		// never unmount the kernel root or even try
		vfsUnrefDentry(dref);
		return EPERM;
	};
	
	if ((dref.dent->flags & VFS_DENTRY_MNTPOINT) == 0)
	{
		vfsUnrefDentry(dref);
		return EINVAL;
	};
	
	FileSystem *fs = dref.dent->target->fs;
	
	if (fs->numMounts == 1)
	{
		// the last mount, we can safely lock it without causing a deadlock. also make sure
		// that the inodes in this filesystem are all unused
		semWait(&fs->lock);
		
		int foundBusy = 0;
		Inode *scan;
		for (scan=fs->imap; scan!=NULL; scan=scan->next)
		{
			// if the only reference to the inode is its placement and (for directories) all of its
			// dentries, then nobody has it open.
			int expectedCount = scan->dups;
			
			if (semWaitGen(&scan->lock, 1, SEM_W_NONBLOCK, 0) != 1)
			{
				// if we couldn't immediately lock the inode, it means someone's using it
				// (and we can't block waiting for them since most threads will lock the inode
				// before locking the filesystem if necessary, so we could deadlock)
				foundBusy = 1;
				break;
			};
			
			Dentry *dent;
			for (dent=scan->dents; dent!=NULL; dent=dent->next)
			{
				expectedCount++;
			};
			
			if (scan->refcount != expectedCount)
			{
				foundBusy = 1;
				semSignal(&scan->lock);
				break;
			};
			
			semSignal(&scan->lock);
		};
		
		if (foundBusy)
		{
			semSignal(&fs->lock);
			vfsUnrefDentry(dref);
			return EBUSY;
		};
		
		// not busy, we can remove the filesystem.
		// first flush and free all inodes
		fs->unmounting = 1;
		while (fs->imap != NULL)
		{
			scan = fs->imap;
			fs->imap = scan->next;
			
			// TODO: perhaps offer some sort of flag that might allow for recovery when the flushing
			// fails?
			vfsFlush(scan);
			
			// delete all dentries
			// do NOT downref their targets - they are on the imap and this loop will catch them.
			// do not interfere with the loop! except if they belong to another filesystem then yes
			while (scan->dents != NULL)
			{
				Dentry *dent = scan->dents;
				scan->dents = dent->next;
				
				if (dent->target != NULL)
				{
					if (dent->target->fs != fs)
					{
						vfsDownrefInode(dent->target);
					};
				};
				
				kfree(dent->name);
				kfree(dent);
				
				vfsDownrefInode(scan);		// == dent->dir
			};
			
			vfsDownrefInode(scan);
		};
		
		if (fs->unmount != NULL) fs->unmount(fs);
		kfree(fs);
	}
	else
	{
		__sync_fetch_and_add(&fs->numMounts, -1);
	};

	// making a mountpoint on a dentry uprefs its parent directory to ensure that filesystems holding other
	// mountpoints cannot be unmounted (see vfsMount() ), so downref it here now that the mountpoint was removed
	vfsDownrefInode(dref.dent->dir);
	
	dref.dent->target = NULL;
	dref.dent->flags &= ~VFS_DENTRY_MNTPOINT;
	vfsUnrefDentry(dref);
	return 0;
};

struct Semaphore_* vfsGetConstSem()
{
	return &semConst;
};

int vfsInodeChangeXPerm(Inode *inode, uint64_t ixperm, uint64_t oxperm, uint64_t dxperm)
{
	if (!havePerm(XP_CHXPERM))
	{
		ERRNO = EACCES;
		return -1;
	};
	
	if (ixperm != XP_NCHG)
	{
		if ((getCurrentThread()->dxperm & ixperm) != ixperm)
		{
			ERRNO = EACCES;
			return -1;
		};
	};

	if (oxperm != XP_NCHG)
	{
		if ((getCurrentThread()->dxperm & oxperm) != oxperm)
		{
			ERRNO = EACCES;
			return -1;
		};
	};

	if (dxperm != XP_NCHG)
	{
		if ((getCurrentThread()->dxperm & dxperm) != dxperm)
		{
			ERRNO = EACCES;
			return -1;
		};
	};
	
	semWait(&inode->lock);
	inode->ixperm = ixperm;
	inode->oxperm = oxperm;
	inode->dxperm = dxperm;
	vfsDirtyInode(inode);
	semSignal(&inode->lock);
	
	return 0;
};

int vfsChangeXPerm(InodeRef startdir, const char *path, uint64_t ixperm, uint64_t oxperm, uint64_t dxperm)
{
	int error;
	DentryRef dref = vfsGetDentry(startdir, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		ERRNO = error;
		return -1;
	};
	
	InodeRef iref = vfsGetInode(dref, 1, &error);
	if (iref.inode == NULL)
	{
		ERRNO = error;
		return -1;
	};
	
	int ret = vfsInodeChangeXPerm(iref.inode, ixperm, oxperm, dxperm);
	error = ERRNO;
	vfsUnrefInode(iref);
	ERRNO = error;
	return ret;
};

ssize_t vfsReadDir(Inode *inode, int key, struct kdirent **out)
{
	if ((inode->mode & VFS_MODE_TYPEMASK) != VFS_MODE_DIRECTORY)
	{
		return -ENOTDIR;
	};
	
	semWait(&inode->lock);
	
	if (key >= 0 && key < 2)
	{
		struct kdirent *dirent = (struct kdirent*) kmalloc(sizeof(struct kdirent) + 3);
		memset(dirent, 0, sizeof(struct kdirent) + 3);
		
		switch (key)
		{
		case 0:
			dirent->d_ino = inode->ino;
			strcpy(dirent->d_name, ".");
			break;
		case 1:
			if (inode->parent != NULL) dirent->d_ino = inode->parent->dir->ino;
			else dirent->d_ino = inode->ino;
			strcpy(dirent->d_name, "..");
			break;
		};
		
		*out = dirent;
		semSignal(&inode->lock);
		return sizeof(struct kdirent) + 3;
	};
	
	int haveHigher = 0;
	Dentry *dent;
	for (dent=inode->dents; dent!=NULL; dent=dent->next)
	{
		if (dent->key == key)
		{
			struct kdirent *dirent = (struct kdirent*) kmalloc(sizeof(struct kdirent) + strlen(dent->name) + 1);
			memset(dirent, 0, sizeof(struct kdirent) + strlen(dent->name) + 1);
			dirent->d_ino = dent->ino;
			strcpy(dirent->d_name, dent->name);
			*out = dirent;
			semSignal(&inode->lock);
			return sizeof(struct kdirent) + strlen(dirent->d_name) + 1;
		};
		
		if (dent->key > key)
		{
			haveHigher = 1;
		};
	};
	
	semSignal(&inode->lock);
	
	if (haveHigher) return -ENOENT;
	else return -EOVERFLOW;
};
