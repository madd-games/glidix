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

#include <glidix/devfs.h>
#include <glidix/common.h>
#include <glidix/memory.h>
#include <glidix/spinlock.h>
#include <glidix/string.h>
#include <glidix/vfs.h>

typedef struct _DeviceFile
{
	char		name[16];
	void		*data;
	int		(*open)(void *data, File *file, size_t szFile);
	struct stat	st;
	
	struct _DeviceFile *prev;
	struct _DeviceFile *next;
} DeviceFile;

static FileSystem *devfs;
static Spinlock devfsLock;
static DeviceFile nullDevice;

static ssize_t nullWrite(File *file, const void *buffer, size_t size)
{
	return (ssize_t) size;
};

static int openNullDevice(void *data, File *file, size_t szFile)
{
	file->write = nullWrite;
	return 0;
};

static int openfile(Dir *dir, File *file, size_t szFile)
{
	DeviceFile *dev = (DeviceFile*) dir->fsdata;
	return dev->open(dev->data, file, szFile);
};

static void close(Dir *dir)
{
	spinlockRelease(&devfsLock);
};

static int next(Dir *dir)
{
	DeviceFile *dev = (DeviceFile*) dir->fsdata;
	if (dev->next == NULL) return -1;
	dev = dev->next;
	dir->fsdata = dev;

	strcpy(dir->dirent.d_name, dev->name);
	dir->dirent.d_ino++;
	dir->stat.st_ino++;
	dir->stat.st_mode = dev->st.st_mode;
	dir->stat.st_uid = dev->st.st_uid;
	dir->stat.st_gid = dev->st.st_gid;
	return 0;
};

static int openroot(FileSystem *fs, Dir *dir, size_t szDir)
{
	if (spinlockTry(&devfsLock))
	{
		// it's locked by someone else.
		return VFS_BUSY;
	};
	dir->fsdata = &nullDevice;
	dir->dirent.d_ino = 0;
	strcpy(dir->dirent.d_name, "null");
	dir->stat.st_dev = 0;
	dir->stat.st_ino = 0;
	dir->stat.st_mode = 0666 | VFS_MODE_CHARDEV;
	dir->stat.st_nlink = 1;
	dir->stat.st_uid = 0;
	dir->stat.st_gid = 0;
	dir->stat.st_rdev = 0;
	dir->stat.st_size = 0;
	dir->stat.st_blocks = 0;
	dir->stat.st_blksize = 0;
	dir->stat.st_mtime = 0;
	dir->stat.st_atime = 0;
	dir->stat.st_ctime = 0;
	dir->openfile = openfile;
	dir->close = close;
	dir->next = next;
	return 0;
};

void initDevfs()
{
	spinlockRelease(&devfsLock);
	devfs = (FileSystem*) kmalloc(sizeof(FileSystem));
	memset(devfs, 0, sizeof(FileSystem));
	devfs->fsname = "devfs";
	devfs->openroot = openroot;

	strcpy(nullDevice.name, "null");
	nullDevice.data = NULL;
	nullDevice.open = openNullDevice;
	nullDevice.prev = NULL;
	nullDevice.next = NULL;
};

FileSystem *getDevfs()
{
	return devfs;
};

Device AddDevice(const char *name, void *data, int (*open)(void*, File*, size_t), int flags)
{
	if (strlen(name) > 15)
	{
		return NULL;
	};

	DeviceFile *dev = (DeviceFile*) kmalloc(sizeof(DeviceFile));
	strcpy(dev->name, name);
	dev->data = data;
	dev->open = open;
	dev->st.st_mode = (mode_t) (flags & 0xFFFF);
	if (flags == 0) dev->st.st_mode = 0644 | VFS_MODE_CHARDEV;
	dev->prev = NULL;
	dev->next = NULL;

	dev->st.st_dev = 0;
	dev->st.st_ino = 0;
	dev->st.st_nlink = 1;
	dev->st.st_uid = 0;
	dev->st.st_gid = 0;
	dev->st.st_rdev = 0;
	dev->st.st_size = 0;
	dev->st.st_blocks = 0;
	dev->st.st_blksize = 0;
	dev->st.st_mtime = 0;
	dev->st.st_atime = 0;
	dev->st.st_ctime = 0;

	DeviceFile *last = &nullDevice;
	while (last->next != NULL) last = last->next;
	last->next = dev;
	dev->prev = last;
	spinlockRelease(&devfsLock);

	return dev;
};

void DeleteDevice(Device ptr)
{
	spinlockAcquire(&devfsLock);
	DeviceFile *dev = (DeviceFile*) ptr;
	if (dev->prev != NULL) dev->prev->next = dev->next;
	if (dev->next != NULL) dev->next->prev = dev->prev;
	if (dev->data != NULL) kfree(dev->data);
	spinlockRelease(&devfsLock);

	kfree(dev);
};

void SetDeviceCreds(Device ptr, uid_t uid, gid_t gid)
{
	spinlockAcquire(&devfsLock);
	DeviceFile *dev = (DeviceFile*) ptr;
	dev->st.st_uid = uid;
	dev->st.st_gid = gid;
	spinlockRelease(&devfsLock);
};
