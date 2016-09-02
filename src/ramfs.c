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

#include <glidix/ramfs.h>
#include <glidix/console.h>
#include <glidix/fsdriver.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/time.h>

/**
 * RAM filesystem list lock.
 */
static Semaphore semRamList;

/**
 * Head of the RAM filesystem list.
 */
static Ramfs *ramList;

typedef struct
{
	RamfsInode*			dirInode;
	RamfsDirent*			currentEntry;
	Ramfs*				ramfs;
} DirData;

typedef struct
{
	RamfsInode*			inode;
	Ramfs*				ramfs;
	
	off_t				pos;
} FileData;

static int ramfs_opendir(Ramfs *ramfs, RamfsInode *inode, Dir *dir, size_t szdir);

static int ramfs_unmount(FileSystem *fs)
{
	return 0;
};

static void nodeDown(RamfsInode *inode)
{
	if (__sync_add_and_fetch(&inode->meta.st_nlink, -1) == 0)
	{
		kfree(inode->data);
		kfree(inode);
	};
};

static void ramdir_close(Dir *dir)
{
	DirData *data = (DirData*) dir->fsdata;
	semWait(&data->ramfs->lock);
	nodeDown(data->dirInode);
	semSignal(&data->ramfs->lock);
	kfree(data);
};

static int ramdir_next(Dir *dir)
{
	DirData *data = (DirData*) dir->fsdata;
	
	semWait(&data->ramfs->lock);
	
	int status;
	while (1)
	{
		RamfsDirent *nextEntry = data->currentEntry->next;
		data->currentEntry = nextEntry;
	
		if (data->currentEntry->dent.d_ino == 1)
		{
			continue;
		};
		
		RamfsInode *entNode = (RamfsInode*) data->currentEntry->dent.d_ino;
		if (entNode != NULL)
		{
			memcpy(&dir->dirent, &data->currentEntry->dent, sizeof(struct dirent));
			memcpy(&dir->stat, &entNode->meta, sizeof(struct stat));
			status = 0;
		}
		else
		{
			status = -1;
		};
		
		break;
	};
	
	semSignal(&data->ramfs->lock);
	return status;
};

static int ramdir_mkdir(Dir *dir, const char *name, mode_t mode, uid_t uid, gid_t gid)
{
	DirData *data = (DirData*) dir->fsdata;
	
	semWait(&data->ramfs->lock);
	
	if (data->currentEntry->dent.d_ino == 1)
	{
		semSignal(&data->ramfs->lock);
		return -1;
	};
	
	RamfsInode *newInode = NEW(RamfsInode);
	memset(newInode, 0, sizeof(RamfsInode));
	newInode->meta.st_ino = (ino_t) newInode;
	newInode->meta.st_mode = mode | VFS_MODE_DIRECTORY;
	newInode->meta.st_nlink = 1;
	newInode->meta.st_uid = uid;
	newInode->meta.st_gid = gid;
	newInode->meta.st_blksize = 512;
	newInode->meta.st_atime = newInode->meta.st_ctime = newInode->meta.st_mtime = time();
	
	RamfsDirent *borderEnts = (RamfsDirent*) kmalloc(sizeof(RamfsDirent)*2);
	memset(borderEnts, 0, sizeof(RamfsDirent)*2);
	borderEnts[0].next = &borderEnts[1];
	borderEnts[1].prev = &borderEnts[0];
	newInode->data = borderEnts;
	
	RamfsDirent *newEnt = NEW(RamfsDirent);
	strcpy(newEnt->dent.d_name, name);
	newEnt->dent.d_ino = (ino_t) newInode;
	newEnt->prev = data->currentEntry->prev;
	newEnt->next = data->currentEntry;
	data->currentEntry->prev->next = newEnt;
	data->currentEntry->prev = newEnt;
	data->currentEntry = newEnt;
	
	data->dirInode->meta.st_size++;
	data->dirInode->meta.st_mtime = time();
	
	semSignal(&data->ramfs->lock);
	return 0;
};

static int ramdir_mkreg(Dir *dir, const char *name, mode_t mode, uid_t uid, gid_t gid)
{
	DirData *data = (DirData*) dir->fsdata;
	
	semWait(&data->ramfs->lock);
	
	if (data->currentEntry->dent.d_ino == 1)
	{
		semSignal(&data->ramfs->lock);
		kprintf("mkreg: inode 1 memes\n");
		return -1;
	};
	
	RamfsInode *newInode = NEW(RamfsInode);
	memset(newInode, 0, sizeof(RamfsInode));
	newInode->meta.st_ino = (ino_t) newInode;
	newInode->meta.st_mode = mode;
	newInode->meta.st_nlink = 1;
	newInode->meta.st_uid = uid;
	newInode->meta.st_gid = gid;
	newInode->meta.st_blksize = 512;
	newInode->meta.st_atime = newInode->meta.st_ctime = newInode->meta.st_mtime = time();
	newInode->data = NULL;
	
	RamfsDirent *newEnt = NEW(RamfsDirent);
	strcpy(newEnt->dent.d_name, name);
	newEnt->dent.d_ino = (ino_t) newInode;
	newEnt->prev = data->currentEntry->prev;
	newEnt->next = data->currentEntry;
	data->currentEntry->prev->next = newEnt;
	data->currentEntry->prev = newEnt;
	data->currentEntry = newEnt;
	
	data->dirInode->meta.st_size++;
	data->dirInode->meta.st_mtime = time();
	
	semSignal(&data->ramfs->lock);
	return 0;
};

static int ramdir_opendir(Dir *me, Dir *dir, size_t szdir)
{
	DirData *data = (DirData*) me->fsdata;
	semWait(&data->ramfs->lock);

	if (data->currentEntry->dent.d_ino == 1)
	{
		semSignal(&data->ramfs->lock);
		return -1;
	};

	int status = ramfs_opendir(data->ramfs, (RamfsInode*)data->currentEntry->dent.d_ino, dir, szdir);
	semSignal(&data->ramfs->lock);
	return status;
};

static int ramdir_chmod(Dir *dir, mode_t mode)
{
	DirData *data = (DirData*) dir->fsdata;
	semWait(&data->ramfs->lock);
	if (data->currentEntry->dent.d_ino == 1)
	{
		semSignal(&data->ramfs->lock);
		return -1;
	};
	
	RamfsInode *inode = (RamfsInode*) data->currentEntry->dent.d_ino;
	inode->meta.st_mode = (inode->meta.st_mode & 0xF000) | (mode & 0xFFF);
	inode->meta.st_ctime = time();
	semSignal(&data->ramfs->lock);
	return 0;
};

static int ramdir_chown(Dir *dir, uid_t uid, gid_t gid)
{
	DirData *data = (DirData*) dir->fsdata;
	semWait(&data->ramfs->lock);
	if (data->currentEntry->dent.d_ino == 1)
	{
		semSignal(&data->ramfs->lock);
		return -1;
	};
	RamfsInode *inode = (RamfsInode*) data->currentEntry->dent.d_ino;
	inode->meta.st_uid = uid;
	inode->meta.st_gid = gid;
	inode->meta.st_ctime = time();
	semSignal(&data->ramfs->lock);
	return 0;
};

static int ramdir_utime(Dir *dir, time_t atime, time_t mtime)
{
	DirData *data = (DirData*) dir->fsdata;
	semWait(&data->ramfs->lock);
	if (data->currentEntry->dent.d_ino == 1)
	{
		semSignal(&data->ramfs->lock);
		return -1;
	};
	RamfsInode *inode = (RamfsInode*) data->currentEntry->dent.d_ino;
	inode->meta.st_atime = atime;
	inode->meta.st_mtime = mtime;
	inode->meta.st_ctime = time();
	semSignal(&data->ramfs->lock);
	return 0;
};

static int ramdir_unlink(Dir *dir)
{
	DirData *data = (DirData*) dir->fsdata;
	semWait(&data->ramfs->lock);
	if (data->currentEntry->dent.d_ino == 1)
	{
		semSignal(&data->ramfs->lock);
		return -1;
	};
	
	RamfsInode *inode = (RamfsInode*) data->currentEntry->dent.d_ino;
	
	data->currentEntry->dent.d_name[0] = 0;
	data->currentEntry->dent.d_ino = 1;
	data->dirInode->meta.st_mtime = time();

	nodeDown(inode);
	data->dirInode->meta.st_size--;
	
	semSignal(&data->ramfs->lock);
	return 0;
};

static int ramdir_symlink(Dir *dir, const char *name, const char *path)
{
	DirData *data = (DirData*) dir->fsdata;
	
	semWait(&data->ramfs->lock);
	
	if (data->currentEntry->dent.d_ino == 1)
	{
		semSignal(&data->ramfs->lock);
		return -1;
	};
	
	RamfsInode *newInode = NEW(RamfsInode);
	memset(newInode, 0, sizeof(RamfsInode));
	newInode->meta.st_ino = (ino_t) newInode;
	newInode->meta.st_mode = 0644 | VFS_MODE_LINK;
	newInode->meta.st_nlink = 1;
	newInode->meta.st_uid = getCurrentThread()->creds->euid;
	newInode->meta.st_gid = getCurrentThread()->creds->egid;
	newInode->meta.st_blksize = 512;
	newInode->meta.st_atime = newInode->meta.st_ctime = newInode->meta.st_mtime = time();
	
	newInode->data = kmalloc(strlen(path)+1);
	strcpy((char*)newInode->data, path);
	
	RamfsDirent *newEnt = NEW(RamfsDirent);
	strcpy(newEnt->dent.d_name, name);
	newEnt->dent.d_ino = (ino_t) newInode;
	newEnt->prev = data->currentEntry->prev;
	newEnt->next = data->currentEntry;
	data->currentEntry->prev->next = newEnt;
	data->currentEntry->prev = newEnt;
	data->currentEntry = newEnt;
	
	data->dirInode->meta.st_size++;
	data->dirInode->meta.st_mtime = time();
	
	semSignal(&data->ramfs->lock);
	return 0;
};

static ssize_t ramdir_readlink(Dir *dir, char *buffer)
{
	DirData *data = (DirData*) dir->fsdata;
	semWait(&data->ramfs->lock);
	if (data->currentEntry->dent.d_ino == 1)
	{
		semSignal(&data->ramfs->lock);
		return -1;
	};
	RamfsInode *inode = (RamfsInode*) data->currentEntry->dent.d_ino;
	strcpy(buffer, (const char*)inode->data);
	ssize_t size = (ssize_t) strlen(buffer);
	semSignal(&data->ramfs->lock);
	return size;
};

static int ramdir_link(Dir *dir, const char *name, ino_t ino)
{
	DirData *data = (DirData*) dir->fsdata;
	semWait(&data->ramfs->lock);
	
	RamfsInode *inode = (RamfsInode*) ino;
	__sync_fetch_and_add(&inode->meta.st_nlink, 1);
	inode->meta.st_ctime = time();
	
	RamfsDirent *newEnt = NEW(RamfsDirent);
	strcpy(newEnt->dent.d_name, name);
	newEnt->dent.d_ino = ino;
	newEnt->prev = data->currentEntry->prev;
	newEnt->next = data->currentEntry;
	data->currentEntry->prev->next = newEnt;
	data->currentEntry->prev = newEnt;
	data->currentEntry = newEnt;
	data->dirInode->meta.st_mtime = time();
	
	semSignal(&data->ramfs->lock);
	return 0;
};

static void ramfile_close(File *fp)
{
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->ramfs->lock);
	nodeDown(data->inode);
	semSignal(&data->ramfs->lock);
	kfree(data);
};

static off_t ramfile_seek(File *fp, off_t offset, int whence)
{
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->ramfs->lock);

	switch (whence)
	{
	case SEEK_SET:
		data->pos = offset;
		break;
	case SEEK_END:
		data->pos = (off_t)data->inode->meta.st_size + offset;
		break;
	case SEEK_CUR:
		data->pos += offset;
		break;
	};
	
	if (data->pos < 0) data->pos = 0;
	off_t result = data->pos;
		
	semSignal(&data->ramfs->lock);
	return result;
};

static int ramfile_dup(File *me, File *fp, size_t szfile)
{
	FileData *data = (FileData*) me->fsdata;
	semWait(&data->ramfs->lock);
	
	__sync_fetch_and_add(&data->inode->meta.st_nlink, 1);
	
	memcpy(fp, me, szfile);
	
	FileData *newData = NEW(FileData);
	memcpy(newData, data, sizeof(FileData));
	
	fp->fsdata = newData;
	
	semSignal(&data->ramfs->lock);
	return 0;
};

static int ramfile_fstat(File *fp, struct stat *st)
{
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->ramfs->lock);
	
	memcpy(st, &data->inode->meta, sizeof(struct stat));
	
	semSignal(&data->ramfs->lock);
	return 0;
};

static int ramfile_fchmod(File *fp, mode_t mode)
{
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->ramfs->lock);
	
	data->inode->meta.st_mode = (data->inode->meta.st_mode & 0xF000) | (mode & 0x0FFF);
	data->inode->meta.st_ctime = time();
	
	semSignal(&data->ramfs->lock);
	return 0;
};

static int ramfile_fchown(File *fp, uid_t uid, gid_t gid)
{
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->ramfs->lock);
	
	data->inode->meta.st_uid = uid;
	data->inode->meta.st_gid = gid;
	data->inode->meta.st_ctime = time();
	
	semSignal(&data->ramfs->lock);
	return 0;
};

static void ramfile_pollinfo(File *fp, Semaphore **sems)
{
	sems[PEI_READ] = vfsGetConstSem();
	sems[PEI_WRITE] = vfsGetConstSem();
};

static ssize_t ramfile_pread(File *fp, void *buffer, size_t size, off_t off)
{
	if (off < 0)
	{
		return 0;
	};
	
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->ramfs->lock);
	
	if (off > (off_t)data->inode->meta.st_size)
	{
		semSignal(&data->ramfs->lock);
		return 0;
	};
	
	size_t maxSize = data->inode->meta.st_size - (size_t) off;
	if (size > maxSize) size = maxSize;
	
	memcpy(buffer, (char*)data->inode->data + off, size);
	data->inode->meta.st_atime = time();
	semSignal(&data->ramfs->lock);
	
	return (ssize_t) size;
};

static ssize_t ramfile_read(File *fp, void *buffer, size_t size)
{
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->ramfs->lock);
	
	if (data->pos > (off_t)data->inode->meta.st_size)
	{
		semSignal(&data->ramfs->lock);
		return 0;
	};
	
	size_t maxSize = data->inode->meta.st_size - (size_t) data->pos;
	if (size > maxSize) size = maxSize;
	
	memcpy(buffer, (char*)data->inode->data + data->pos, size);
	data->pos += size;
	data->inode->meta.st_atime = time();
	semSignal(&data->ramfs->lock);
	
	return (ssize_t) size;
};

static ssize_t ramfile_pwrite(File *fp, const void *buffer, size_t size, off_t off)
{
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->ramfs->lock);
	
	size_t minSize = size + (size_t)off;
	if (data->inode->meta.st_size < minSize)
	{
		void *newData = kmalloc(minSize);
		memset(newData, 0, minSize);
		memcpy(newData, data->inode->data, data->inode->meta.st_size);
		kfree(data->inode->data);
		data->inode->data = newData;
		data->inode->meta.st_size = minSize;
	};
	
	memcpy((char*)data->inode->data + off, buffer, size);
	data->inode->meta.st_mtime = data->inode->meta.st_atime = time();
	semSignal(&data->ramfs->lock);
	
	return (ssize_t) size;
};

static ssize_t ramfile_write(File *fp, const void *buffer, size_t size)
{
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->ramfs->lock);
	
	size_t minSize = size + (size_t)data->pos;
	if (data->inode->meta.st_size < minSize)
	{
		void *newData = kmalloc(minSize);
		memset(newData, 0, minSize);
		memcpy(newData, data->inode->data, data->inode->meta.st_size);
		kfree(data->inode->data);
		data->inode->data = newData;
		data->inode->meta.st_size = minSize;
	};
	
	memcpy((char*)data->inode->data + data->pos, buffer, size);
	data->pos += size;
	data->inode->meta.st_mtime = data->inode->meta.st_atime = time();
	semSignal(&data->ramfs->lock);
	
	return (ssize_t) size;
};

static void ramfile_truncate(File *fp, off_t len)
{
	if (len < 0) return;
	
	size_t size = (size_t) len;
	
	FileData *data = (FileData*) fp->fsdata;
	semWait(&data->ramfs->lock);
	
	if (data->inode->meta.st_size == size)
	{
		semSignal(&data->ramfs->lock);
		return;
	};
	
	void *newData = kmalloc(size);
	memset(newData, 0, size);
	
	size_t toCopy = data->inode->meta.st_size;
	if (toCopy > size) toCopy = size;
	memcpy(newData, data->inode->data, toCopy);
	
	kfree(data->inode->data);
	data->inode->data = newData;
	data->inode->meta.st_size = size;
	data->inode->meta.st_ctime = time();
	
	semSignal(&data->ramfs->lock);
};

static int ramdir_openfile(Dir *dir, File *fp, size_t szfile)
{
	DirData *data = (DirData*) dir->fsdata;
	semWait(&data->ramfs->lock);
	if (data->currentEntry->dent.d_ino == 1)
	{
		semSignal(&data->ramfs->lock);
		return -1;
	};
	
	FileData *fdata = NEW(FileData);
	fdata->ramfs = data->ramfs;
	fdata->inode = (RamfsInode*) data->currentEntry->dent.d_ino;
	fdata->pos = 0;
	
	__sync_fetch_and_add(&fdata->inode->meta.st_nlink, 1);

	fp->fsdata = fdata;
	fp->close = ramfile_close;
	fp->seek = ramfile_seek;
	fp->dup = ramfile_dup;
	fp->fstat = ramfile_fstat;
	fp->fchmod = ramfile_fchmod;
	fp->fchown = ramfile_fchown;
	fp->pollinfo = ramfile_pollinfo;
	fp->pread = ramfile_pread;
	fp->read = ramfile_read;
	fp->pwrite = ramfile_pwrite;
	fp->write = ramfile_write;
	fp->truncate = ramfile_truncate;
	
	semSignal(&data->ramfs->lock);
	return 0;
};

static int ramfs_opendir(Ramfs *ramfs, RamfsInode *inode, Dir *dir, size_t szdir)
{
	// call this function with the filesystem locked!
	DirData *data = NEW(DirData);
	__sync_fetch_and_add(&inode->meta.st_nlink, 1);
	data->dirInode = inode;
	inode->meta.st_atime = time();
	
	data->currentEntry = ((RamfsDirent*)inode->data)->next;
	while (data->currentEntry->dent.d_ino == 1) data->currentEntry = data->currentEntry->next;
	data->ramfs = ramfs;
	
	dir->fsdata = data;
	dir->close = ramdir_close;
	dir->mkdir = ramdir_mkdir;
	dir->next = ramdir_next;
	dir->opendir = ramdir_opendir;
	dir->chmod = ramdir_chmod;
	dir->chown = ramdir_chown;
	dir->utime = ramdir_utime;
	dir->unlink = ramdir_unlink;
	dir->symlink = ramdir_symlink;
	dir->readlink = ramdir_readlink;
	dir->link = ramdir_link;
	dir->mkreg = ramdir_mkreg;
	dir->openfile = ramdir_openfile;
	
	RamfsInode *entNode = (RamfsInode*) data->currentEntry->dent.d_ino;
	if (entNode != NULL)
	{
		memcpy(&dir->dirent, &data->currentEntry->dent, sizeof(struct dirent));
		memcpy(&dir->stat, &entNode->meta, sizeof(struct stat));
	};
	
	if (data->currentEntry->dent.d_ino == 0)
	{
		return VFS_EMPTY_DIRECTORY;
	};
	
	return 0;
};

static int ramfs_openroot(FileSystem *fs, Dir *dir, size_t szdir)
{
	Ramfs *ramfs = (Ramfs*) fs->fsdata;
	semWait(&ramfs->lock);
	int status = ramfs_opendir(ramfs, &ramfs->root, dir, szdir);
	semSignal(&ramfs->lock);
	return status;
};

static int ramfsMount(const char *image, FileSystem *fs, size_t szfs)
{
	if (strlen(image) >= 64)
	{
		return -1;
	};
	
	if (image[0] != '.')
	{
		return -1;
	};
	
	semWait(&semRamList);
	
	Ramfs *ramfs;
	for (ramfs=ramList; ramfs!=NULL; ramfs=ramfs->next)
	{
		if (strcmp(ramfs->name, image) == 0)
		{
			break;
		};
	};
	
	if (ramfs == NULL)
	{
		kprintf_debug("ramfs: creating new ramfs called '%s'\n", image);
		ramfs = NEW(Ramfs);
		strcpy(ramfs->name, image);
		ramfs->prev = NULL;
		ramfs->next = ramList;
		if (ramList != NULL) ramList->prev = ramfs;
		ramList = ramfs;
		memset(&ramfs->root, 0, sizeof(RamfsInode));
		
		RamfsDirent *ents = (RamfsDirent*) kmalloc(sizeof(RamfsDirent)*2);
		
		RamfsDirent *rootHead = &ents[0];
		RamfsDirent *rootEnd = &ents[1];
		rootHead->prev = NULL;
		rootEnd->prev = rootHead;
		rootHead->next = rootEnd;
		rootEnd->next = NULL;
		rootEnd->dent.d_ino = 0;
		
		ramfs->root.meta.st_ino = (ino_t) &ramfs->root;
		ramfs->root.meta.st_mode = VFS_MODE_DIRECTORY | 0755;
		ramfs->root.meta.st_nlink = 1;
		ramfs->root.meta.st_blksize = 512;
		ramfs->root.meta.st_atime = ramfs->root.meta.st_mtime = ramfs->root.meta.st_ctime = time();
		ramfs->root.data = rootHead;
		
		semInit(&ramfs->lock);
	};
	
	semWait(&ramfs->lock);
	semSignal(&semRamList);
	
	fs->fsdata = ramfs;
	fs->fsname = "ramfs";
	fs->openroot = ramfs_openroot;
	fs->unmount = ramfs_unmount;
	
	semSignal(&ramfs->lock);
	return 0;
};

FSDriver ramDriver = {
	.onMount = ramfsMount,
	.name = "ramfs"
};

void ramfsInit()
{
	kprintf("Initializing the ramfs subsystem... ");
	semInit(&semRamList);
	ramList = NULL;
	registerFSDriver(&ramDriver);
	DONE();
};
