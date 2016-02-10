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

#include <glidix/fdopendir.h>
#include <glidix/vfs.h>
#include <glidix/sched.h>
#include <glidix/spinlock.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/errno.h>
#include <glidix/console.h>

static ssize_t readdir(File *fp, void *buffer, size_t size)
{
	Dir *dir = (Dir*) fp->fsdata;
	if (dir == NULL) return 0;
	if (size > sizeof(struct dirent)) size = sizeof(struct dirent);
	memcpy(buffer, &dir->dirent, size);

	do
	{
		if (dir->next(dir) != 0)
		{
			if (dir->close != NULL) dir->close(dir);
			kfree(dir);
			fp->fsdata = NULL;
			break;
		};
	} while (dir->dirent.d_ino == 0);

	return (ssize_t) size;
};

static void closedir(File *fp)
{
	Dir *dir = (Dir*) fp->fsdata;
	if (dir == NULL) return;
	if (dir->close != NULL) dir->close(dir);
	kfree(dir);
};

int sys_fdopendir(const char *dirname)
{
	char fulldir[512];
	strcpy(fulldir, dirname);
	if (dirname[strlen(dirname)-1] != '/') strcat(fulldir, "/");

	int error;
	Dir *dir = parsePath(fulldir, VFS_CHECK_ACCESS, &error);
	if (dir == NULL)
	{
		switch (error)
		{
		case VFS_NO_FILE:
			getCurrentThread()->therrno = ENOENT;
			break;
		case VFS_PERM:
			getCurrentThread()->therrno = EACCES;
			break;
		case VFS_NOT_DIR:
			getCurrentThread()->therrno = ENOTDIR;
			break;
		case VFS_EMPTY_DIRECTORY:
			return -2;
			break;
		default:
			getCurrentThread()->therrno = EACCES;
			break;
		};
		return -1;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);
	int fd;
	for (fd=0; fd<MAX_OPEN_FILES; fd++)
	{
		if (ftab->entries[fd] == NULL)
		{
			break;
		};
	};

	if (fd == MAX_OPEN_FILES)
	{
		spinlockRelease(&ftab->spinlock);
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		getCurrentThread()->therrno = EMFILE;
		return -1;
	};

	File *fp = (File*) kmalloc(sizeof(File));
	memset(fp, 0, sizeof(File));
	fp->oflag = O_RDONLY;
	fp->fsdata = dir;
	fp->read = readdir;
	fp->close = closedir;
	ftab->entries[fd] = fp;

	while (dir->dirent.d_ino == 0)
	{
		if (dir->next(dir) != 0)
		{
			if (dir->close != NULL) dir->close(dir);
			kfree(dir);
			fp->fsdata = NULL;
			break;
		};
	};

	spinlockRelease(&ftab->spinlock);
	return fd;
};
