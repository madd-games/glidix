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

typedef struct _DeviceFile
{
	char		name[16];
	void		*data;
	int		(*open)(void *data, File *file, size_t szFile);
	struct stat	st;
	ino_t		inode;
	
	struct _DeviceFile *prev;
	struct _DeviceFile *next;
} DeviceFile;

static FileSystem *devfs;
static Mutex devfsLock;
static DeviceFile nullDevice;

static ssize_t nullWrite(File *file, const void *buffer, size_t size)
{
	return (ssize_t) size;
};

static ssize_t nullPWrite(File *fp, const void *buffer, size_t size, off_t offset)
{
	return (ssize_t) size;
};

static ssize_t nullRead(File *fp, void *buffer, size_t size)
{
	return 0;
};

static ssize_t nullPRead(File *fp, void *buffer, size_t size, off_t offset)
{
	return 0;
};

static int openNullDevice(void *data, File *file, size_t szFile)
{
	file->write = nullWrite;
	file->read = nullRead;
	file->pwrite = nullPWrite;
	file->pread = nullPRead;
	return 0;
};

static DeviceFile* getDeviceByInode(ino_t inode)
{
	// call this with devfsLock acquired.
	DeviceFile *dev;
	
	for (dev=&nullDevice; dev!=NULL; dev=dev->next)
	{
		if (dev->inode == inode) break;
	};
	
	return dev;
};

static int openfile(Dir *dir, File *file, size_t szFile)
{
	mutexLock(&devfsLock);
	DeviceFile *dev = getDeviceByInode(dir->dirent.d_ino);
	if (dev == NULL)
	{
		mutexUnlock(&devfsLock);
		return VFS_BUSY;
	};

	int status = dev->open(dev->data, file, szFile);
	mutexUnlock(&devfsLock);
	return status;
};

static int loadDeviceInfo(Dir *dir, ino_t inode);

static int next(Dir *dir)
{
	mutexLock(&devfsLock);
	
	DeviceFile *current = getDeviceByInode(dir->dirent.d_ino);
	if (current == NULL)
	{
		mutexUnlock(&devfsLock);
		return -1;
	};
	
	DeviceFile *next = current->next;
	if (next == NULL)
	{
		mutexUnlock(&devfsLock);
		return -1;
	};
	
	ino_t inode = next->inode;
	mutexUnlock(&devfsLock);
	
	return loadDeviceInfo(dir, inode);
};

static int loadDeviceInfo(Dir *dir, ino_t inode)
{
	mutexLock(&devfsLock);
	dir->dirent.d_ino = inode;
	
	DeviceFile *dev = getDeviceByInode(inode);
	if (dev == NULL)
	{
		mutexUnlock(&devfsLock);
		return -1;
	};
	
	strcpy(dir->dirent.d_name, dev->name);
	dir->stat.st_dev = 0;
	dir->stat.st_ino = inode;
	dir->stat.st_nlink = 1;
	dir->stat.st_rdev = 0;
	dir->stat.st_size = 0;
	dir->stat.st_blocks = 0;
	dir->stat.st_blksize = 0;
	dir->stat.st_mtime = 0;
	dir->stat.st_atime = 0;
	dir->stat.st_ctime = 0;
	dir->stat.st_mode = dev->st.st_mode;
	dir->stat.st_uid = dev->st.st_uid;
	dir->stat.st_gid = dev->st.st_gid;

	dir->openfile = openfile;
	dir->next = next;
	mutexUnlock(&devfsLock);
	return 0;
};

static int openroot(FileSystem *fs, Dir *dir, size_t szDir)
{
	return loadDeviceInfo(dir, 7);
};

static ssize_t zeroWrite(File *fp, const void *buf, size_t size)
{
	return (ssize_t) size;
};

static ssize_t zeroPWrite(File *fp, const void *buf, size_t size, off_t offset)
{
	return (ssize_t) size;
};

static ssize_t zeroRead(File *fp, void *buf, size_t size)
{
	memset(buf, 0, size);
	return (ssize_t) size;
};

static ssize_t zeroPRead(File *fp, void *buf, size_t size, off_t offset)
{
	memset(buf, 0, size);
	return (ssize_t) size;
};

static int openZero(void *data, File *file, size_t szFile)
{
	file->write = zeroWrite;
	file->read = zeroRead;
	file->pwrite = zeroPWrite;
	file->pread = zeroPRead;
	return 0;
};

static ssize_t randomWrite(File *fp, const void *buf, size_t size)
{
	const uint64_t *scan = (const uint64_t*) buf;
	while (size >= 8)
	{
		feedRandom(*scan++);
		size -= 8;
	};
	
	return (ssize_t) size;
};

static ssize_t randomPWrite(File *fp, const void *buf, size_t size, off_t offset)
{
	return randomWrite(fp, buf, size);
};

static ssize_t randomRead(File *fp, void *buf, size_t size)
{
	uint8_t *put = (uint8_t*) buf;
	ssize_t sizeOut = 0;
	while (size > 0)
	{
		uint64_t num = getRandom();
		
		size_t copyNow = size;
		if (copyNow > 8) copyNow = 8;
		memcpy(put, &num, copyNow);
		put += copyNow;
		size -= copyNow;
		sizeOut += copyNow;
	};
	
	return sizeOut;
};

static ssize_t randomPRead(File *fp, void *buf, size_t size, off_t offset)
{
	return randomRead(fp, buf, size);
};

static int openRandom(void *data, File *file, size_t szFile)
{
	file->write = randomWrite;
	file->read = randomRead;
	file->pwrite = randomPWrite;
	file->pread = randomPRead;
	return 0;
};

void initDevfs()
{
	mutexInit(&devfsLock);
	devfs = (FileSystem*) kmalloc(sizeof(FileSystem));
	memset(devfs, 0, sizeof(FileSystem));
	devfs->fsname = "devfs";
	devfs->openroot = openroot;

	strcpy(nullDevice.name, "null");
	nullDevice.data = NULL;
	nullDevice.open = openNullDevice;
	nullDevice.prev = NULL;
	nullDevice.next = NULL;
	nullDevice.inode = 7;
	
	if (AddDevice("zero", NULL, openZero, 0666 | VFS_MODE_CHARDEV) == NULL)
	{
		panic("failed to create /dev/zero");
	};
	
	if (AddDevice("urandom", NULL, openRandom, 0644 | VFS_MODE_CHARDEV) == NULL)
	{
		panic("failed to create /dev/urandom");
	};
	
	if (AddDevice("random", NULL, openRandom, 0644 | VFS_MODE_CHARDEV) == NULL)
	{
		panic("failed to create /dev/random");
	};
};

FileSystem *getDevfs()
{
	return devfs;
};

static ino_t nextDevIno = 8;
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

	dev->inode = __sync_fetch_and_add(&nextDevIno, 1);
	
	mutexLock(&devfsLock);
	DeviceFile *last = &nullDevice;
	while (last->next != NULL) last = last->next;
	last->next = dev;
	dev->prev = last;
	mutexUnlock(&devfsLock);

	return dev;
};

void DeleteDevice(Device ptr)
{
	mutexLock(&devfsLock);
	DeviceFile *dev = (DeviceFile*) ptr;
	if (dev->prev != NULL) dev->prev->next = dev->next;
	if (dev->next != NULL) dev->next->prev = dev->prev;
	if (dev->data != NULL) kfree(dev->data);
	mutexUnlock(&devfsLock);

	kfree(dev);
};

void SetDeviceCreds(Device ptr, uid_t uid, gid_t gid)
{
	mutexLock(&devfsLock);
	DeviceFile *dev = (DeviceFile*) ptr;
	dev->st.st_uid = uid;
	dev->st.st_gid = gid;
	mutexUnlock(&devfsLock);
};
