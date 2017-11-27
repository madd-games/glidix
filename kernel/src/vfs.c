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

DentryRef VFS_NULL_DREF = {NULL, NULL};
InodeRef VFS_NULL_IREF = {NULL, NULL};

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
			if (fs->regInode(fs, inode) != 0)
			{
				kfree(inode);
				return NULL;
			};
		};
		semSignal(&fs->lock);
	};
	
	inode->flags |= VFS_INODE_DIRTY;
	return inode;
};

void vfsDirtyInode(Inode *inode)
{
	inode->flags |= VFS_INODE_DIRTY;
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

static void vfsCallInInode(Dentry *dent)
{
	if (dent->target == NULL)
	{
		FileSystem *fs = dent->dir->fs;
		if (fs == NULL)
		{
			panic("dentry cache inconsistent: dentry without cached target inode or backing filesystem");
		};
		
		Inode *inode = NEW(Inode);
		memset(inode, 0, sizeof(Inode));
		inode->refcount = 1;
		semInit(&inode->lock);
		inode->fs = fs;
		inode->parent = dent;

		semWait(&fs->lock);
		if (fs->loadInode == NULL)
		{
			panic("dentry without cached target inode, and backing filesytem driver cannot load inodes");
		};
		
		if (fs->loadInode(fs, dent, inode) != 0)
		{
			semSignal(&fs->lock);
			kfree(inode);
			return;
		};
		
		dent->target = inode;
	};
};

InodeRef vfsGetDentryTarget(DentryRef dref)
{
	// TODO: cross mountpoints
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
	// TODO: determine the current process root
	vfsUprefInode(kernelRootInode);
	
	InodeRef iref;
	iref.inode = kernelRootInode;
	iref.top = NULL;
	
	return iref;
};

InodeRef vfsGetCurrentDir()
{
	// TODO: determine the current process working directory
	return vfsGetRoot();
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
	
	semWait(&inode->lock);
	
	// check if the current user is listed in the ACL
	int i;
	for (i=0; i<VFS_ACL_SIZE; i++)
	{
		if (inode->acl[i].ace_type == VFS_ACE_USER && inode->acl[i].ace_id == uid)
		{
			semSignal(&inode->lock);
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
	semSignal(&inode->lock);
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
		
		// now the inode must be a directory
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
		if (!vfsIsAllowed(dir.inode, neededPerms))
		{
			vfsUnrefInode(dir);
			kfree(pathbuf);
			if (error != NULL) *error = EACCES;
			return nulref;
		};
		
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
		if (inode->ft != NULL)
		{
			ftUp(inode->ft);
			ftUncache(inode->ft);
			ftDown(inode->ft);
			inode->ft = NULL;
		};
		
		if (inode->drop != NULL)
		{
			inode->drop(inode);
		};
	};
	
	semSignal(&inode->lock);
	vfsDownrefInode(inode);
};

int vfsUnlinkInode(DentryRef dref, int flags)
{
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
	
	// if we do not have the XP_FSADMIN permission, then make sure we are allowed to
	// unlink this entry
	if (!havePerm(XP_FSADMIN))
	{
		if (dref.dent->dir->mode & VFS_MODE_STICKY)
		{
			// if the parent directory has the sticky bit set, then the owner of either
			// the parent directory or the entry itself may delete it
			uid_t uid = 0;
			if (getCurrentThread()->creds != NULL) uid = getCurrentThread()->creds->euid;
			
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
