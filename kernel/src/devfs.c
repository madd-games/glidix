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

static ssize_t null_pread(Inode *inode, File *fp, void *buffer, size_t size, off_t offset)
{
	return 0;
};

static ssize_t null_pwrite(Inode *inode, File *fp, const void *buffer, size_t size, off_t offset)
{
	return (ssize_t) size;
};

static ssize_t zero_pread(Inode *inode, File *fp, void *buffer, size_t size, off_t offset)
{
	memset(buffer, 0, size);
	return (ssize_t) size;
};

static ssize_t zero_pwrite(Inode *inode, File *fp, const void *buffer, size_t size, off_t offset)
{
	return (ssize_t) size;
};

static ssize_t random_pread(Inode *inode, File *fp, void *buffer, size_t size, off_t offset)
{
	uint64_t *put = (uint64_t*) buffer;
	ssize_t out = 0;
	while (size >= 8)
	{
		*put++ = getRandom();
		size -= 8;
		out += 8;
	};
	
	if (size != 0)
	{
		uint64_t final = getRandom();
		memcpy(put, &final, size);
		out += size;
	};
	
	return out;
};

static ssize_t random_pwrite(Inode *inode, File *fp, const void *buffer, size_t size, off_t offset)
{
	const uint64_t *scan = (const uint64_t*) buffer;
	ssize_t out = 0;
	while (size >= 8)
	{
		feedRandom(*scan++);
		size -= 8;
		out += 8;
	};
	
	if (size != 0)
	{
		uint64_t final = getRandom();
		memcpy(&final, scan, size);
		feedRandom(final);
		out += size;
	};
	
	return out;
};

void initDevfs()
{
	kprintf("Creating /dev... ");
	if (vfsMakeDir(VFS_NULL_IREF, "/dev", 0755) != 0)
	{
		FAILED();
		panic("could not create /dev");
	};
	
	DONE();
	
	kprintf("Creating /dev/null... ");
	Inode *inodeNull = vfsCreateInode(NULL, VFS_MODE_CHARDEV | 0666);
	inodeNull->pread = null_pread;
	inodeNull->pwrite = null_pwrite;
	if (devfsAdd("null", inodeNull) != 0)
	{
		FAILED();
		panic("could not register /dev/null");
	};
	DONE();
	
	kprintf("Creating /dev/zero... ");
	Inode *inodeZero = vfsCreateInode(NULL, VFS_MODE_CHARDEV | 0666);
	inodeZero->pread = zero_pread;
	inodeZero->pwrite = zero_pwrite;
	if (devfsAdd("zero", inodeZero) != 0)
	{
		FAILED();
		panic("could not register /dev/null");
	};
	DONE();
	
	kprintf("Creating /dev/random and /dev/urandom... ");
	Inode *inodeRandom = vfsCreateInode(NULL, VFS_MODE_CHARDEV | 0644);
	inodeRandom->pread = random_pread;
	inodeRandom->pwrite = random_pwrite;
	if (devfsAdd("random", inodeRandom) != 0)
	{
		FAILED();
		panic("could not register /dev/random");
	};
	if (devfsAdd("urandom", inodeRandom) != 0)
	{
		FAILED();
		panic("could not register /dev/urandom");
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
	
	// make this independent of root
	Creds *creds = getCurrentThread()->creds;
	getCurrentThread()->creds = NULL;
	
	int error;
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, fullpath, 1, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		getCurrentThread()->creds = creds;
		return error;
	};
	
	if (dref.dent->ino != 0)
	{
		vfsRemoveDentry(dref);
		getCurrentThread()->creds = creds;
		return EEXIST;
	};
	
	vfsBindInode(dref, inode);
	vfsDownrefInode(inode);		// since the above uprefs it, and we want to take the reference

	getCurrentThread()->creds = creds;
	return 0;
};

void devfsRemove(const char *name)
{
	// make this independent of root
	Creds *creds = getCurrentThread()->creds;
	getCurrentThread()->creds = NULL;
	
	char fullpath[256];
	strformat(fullpath, 256, "/dev/%s", name);
	
	int error;
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, fullpath, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		getCurrentThread()->creds = creds;
		return;
	};
	
	vfsUnlinkInode(dref, 0);
	getCurrentThread()->creds = creds;
};
