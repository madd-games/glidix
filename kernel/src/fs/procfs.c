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

#include <glidix/fs/procfs.h>
#include <glidix/util/memory.h>
#include <glidix/thread/sched.h>
#include <glidix/util/string.h>
#include <glidix/display/console.h>

/**
 * Inode for /proc/self.
 */
static Inode *inodeProcSelf;

/**
 * Buffer for the PID string, used as a target for /proc/self. Since each CPU is running a
 * different thread, this must be per-CPU.
 */
static PER_CPU char pidstrbuf[16];

static int procfsRegInode(FileSystem *fs, Inode *inode)
{
	static ino_t nextIno = 10;
	inode->ino = __sync_fetch_and_add(&nextIno, 1);
	return 0;
};

void initProcfs()
{
	FileSystem *procfs = vfsCreateFileSystem("procfs");
	assert(procfs != NULL);
	procfs->regInode = procfsRegInode;
	
	Inode *inoProc = vfsCreateInode(procfs, 0755 | VFS_MODE_DIRECTORY);
	assert(inoProc != NULL);
	
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, "/proc", 1, NULL);
	assert(dref.dent != NULL);
	assert(dref.dent->ino == 0);
	vfsBindInode(dref, inoProc);
	
	assert(vfsCreateSymlink("DANGLING", VFS_NULL_IREF, "/proc/self") == 0);
	
	// get a handle to the /proc/self inode
	dref = vfsGetDentry(VFS_NULL_IREF, "/proc/self", 0, NULL);
	assert(dref.dent != NULL);
	InodeRef iref = vfsGetInode(dref, 0, NULL);
	assert(iref.inode != NULL);
	inodeProcSelf = iref.inode;
	vfsUprefInode(inodeProcSelf);
	vfsUnrefInode(iref);
	
	// set up the /proc/self inode
	inodeProcSelf->flags |= VFS_INODE_NOUNLINK;
	kfree(inodeProcSelf->target);
	inodeProcSelf->target = pidstrbuf;
	
	// mark procfs read-only
	procfs->flags |= VFS_ST_RDONLY;
};

void procfsSetPid(int pid)
{
	strformat(pidstrbuf, 16, "%d", pid);
};

void procfsCreate(int old, int new)
{
	char fullpath[64];
	char parentpath[64];
	strformat(fullpath, 64, "/proc/%d", new);
	
	Creds *creds = getCurrentThread()->creds;
	getCurrentThread()->creds = NULL;
	int error = vfsMakeDirEx(VFS_NULL_IREF, fullpath, 0555, VFS_MKDIR_NOVALID);
	if (error != 0)
	{
		panic("could not create %s: errno %d", fullpath, error);
	};
	
	strformat(fullpath, 64, "/proc/%d/parent", new);
	strformat(parentpath, 64, "../%d", old);
	
	error = vfsCreateSymlinkEx(parentpath, VFS_NULL_IREF, fullpath, VFS_AT_NOVALID);
	if (error != 0)
	{
		panic("could not create %s poiting to %s: errno %d", fullpath, parentpath, error);
	};
	
	// get the EXE path
	char oldexepath[256];
	strformat(oldexepath, 64, "/proc/%d/exe", old);
	char *target = vfsReadLinkPath(VFS_NULL_IREF, oldexepath);
	if (target == NULL) target = strdup("?");
	if (new == 1) target = strdup("/initrd/init");
	
	strformat(fullpath, 64, "/proc/%d/exe", new);
	error = vfsCreateSymlinkEx(target, VFS_NULL_IREF, fullpath, VFS_AT_NOVALID);
	if (error != 0)
	{
		panic("could not create %s: errno %d", fullpath, error);
	};
	kfree(target);
	
	getCurrentThread()->creds = creds;
};

void procfsDelete(int pid)
{
	char fullpath[64];
	
	// delete /proc/<pid>/parent
	strformat(fullpath, 64, "/proc/%d/parent", pid);
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, fullpath, 0, NULL);
	if (dref.dent == NULL)
	{
		panic("could not delete %s", fullpath);
		vfsUnrefDentry(dref);
		return;
	};
	
	int error = vfsUnlinkInode(dref, VFS_AT_NOVALID);
	if (error != 0)
	{
		panic("could not unlink %s: errno %d", fullpath, error);
	};

	// delete /proc/<pid>/exe
	strformat(fullpath, 64, "/proc/%d/exe", pid);
	dref = vfsGetDentry(VFS_NULL_IREF, fullpath, 0, NULL);
	if (dref.dent == NULL)
	{
		panic("could not delete %s", fullpath);
		vfsUnrefDentry(dref);
		return;
	};
	
	error = vfsUnlinkInode(dref, VFS_AT_NOVALID);
	if (error != 0)
	{
		panic("could not unlink %s: errno %d", fullpath, error);
	};

	// delete /proc/<pid>
	strformat(fullpath, 64, "/proc/%d", pid);
	
	dref = vfsGetDentry(VFS_NULL_IREF, fullpath, 0, NULL);
	if (dref.dent == NULL)
	{
		panic("could not delete %s", fullpath);
		vfsUnrefDentry(dref);
		return;
	};
	
	error = vfsUnlinkInode(dref, VFS_AT_REMOVEDIR | VFS_AT_NOVALID);
	if (error != 0)
	{
		panic("could not unlink %s: errno %d", fullpath, error);
	};
};
