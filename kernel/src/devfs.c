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

#include <glidix/devfs.h>
#include <glidix/common.h>
#include <glidix/memory.h>
#include <glidix/mutex.h>
#include <glidix/string.h>
#include <glidix/vfs.h>
#include <glidix/random.h>
#include <glidix/console.h>

void initDevfs()
{
	kprintf("Creating /dev... ");
	if (vfsMakeDir(VFS_NULL_IREF, "/dev", 0755) != 0)
	{
		FAILED();
		panic("could not create /dev");
	};
	
	DONE();
};

int devfsAdd(const char *name, Inode *inode)
{
	if (strlen(name) > 64)
	{
		return ENAMETOOLONG;
	};
	
	char fullpath[256];
	strformat(fullpath, 256, "/dev/%s", name);
	
	int error;
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, fullpath, 1, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		return error;
	};
	
	if (dref.dent->ino != 0)
	{
		vfsRemoveDentry(dref);
		return EEXIST;
	};
	
	vfsBindInode(dref, inode);
	vfsDownrefInode(inode);		// since the above uprefs it, and we want to take the reference
	return 0;
};

void devfsRemove(const char *name)
{
	char fullpath[256];
	strformat(fullpath, 256, "/dev/%s", name);
	
	int error;
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, fullpath, 0, &error);
	if (dref.dent == NULL)
	{
		panic("devfsRemove: unexpected attempt to remove nonexistent device file `%s'", name);
	};
	
	vfsUnlinkInode(dref, 0);
};
