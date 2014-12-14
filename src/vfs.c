/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
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

static void dumpDir(Dir *dir, int prefix)
{
	while (1)
	{
		int count = prefix;
		while (count--) kprintf("  ");

		if (dir->stat.st_mode & VFS_MODE_DIRECTORY)
		{
			kprintf("%s/\n", dir->dirent.d_name);
			Dir subdir;
			if (dir->opendir(dir, &subdir, sizeof(Dir)) == 0)
			{
				dumpDir(&subdir, prefix+1);
			};
		}
		else
		{
			kprintf("%s (%d bytes)\n", dir->dirent.d_name, dir->stat.st_size);
		};

		if (dir->next(dir) != 0) break;
	};	
};

void dumpFS(FileSystem *fs)
{
	kprintf("Filesystem type: %s\n", fs->fsname);

	Dir dir;
	memset(&dir, 0, sizeof(Dir));
	if (fs->openroot == NULL)
	{
		kprintf("This filesystem cannot be opened\n");
	}
	else
	{
		int status = fs->openroot(fs, &dir, sizeof(Dir));

		if (status == VFS_EMPTY_DIRECTORY)
		{
			kprintf("The filesystem root is empty.\n");
		}
		else if (status != 0)
		{
			kprintf("Error opening filesystem root: %d\n", status);
		}
		else
		{
			dumpDir(&dir, 0);
		};
	};

	if (dir.close != NULL) dir.close(&dir);
};

int vfsCanCurrentThread(struct stat *st, mode_t mask)
{
	Thread *thread = getCurrentThread();

	// the root special case (he can do literally anything).
	if (thread->euid == 0)
	{
		return 1;
	};

	if (thread->euid == st->st_uid)
	{
		return ((st->st_mode >> 6) & mask) != 0;
	}
	else if (thread->egid == st->st_gid)
	{
		return ((st->st_mode >> 3) & mask) != 0;
	}
	else
	{
		return (st->st_mode & mask) != 0;
	};
};

Dir *parsePath(const char *path, int flags, int *error)
{
	*error = VFS_NO_FILE;			// default error
	// TODO: relative paths
	if (path[0] != '/')
	{
		return NULL;
	};

	if (path[strlen(path)-1] == '/')
	{
		return NULL;
	};

	SplitPath spath;
	if (resolveMounts(path, &spath) != 0)
	{
		return NULL;
	};

	char token[128];
	const char *scan = spath.filename;

	if (spath.fs->openroot == NULL)
	{
		return NULL;
	};

	Dir *dir = (Dir*) kmalloc(sizeof(Dir));
	memset(dir, 0, sizeof(Dir));
	if (spath.fs->openroot(spath.fs, dir, sizeof(Dir)) != 0)
	{
		kfree(dir);
		return NULL;
	};

	while (1)
	{
		char *put = token;
		while ((*scan != 0) && (*scan != '/'))
		{
			*put++ = *scan++;
		};
		*put = 0;

		if (strlen(token) == 0)
		{
			if (dir->close != NULL) dir->close(dir);
			kfree(dir);
			return NULL;
		};

		while (strcmp(dir->dirent.d_name, token) != 0)
		{
			if (dir->next(dir) != 0)
			{
				if (dir->close != NULL) dir->close(dir);
				kfree(dir);
				return NULL;
			};
		};

		if (*scan == '/')
		{
			if ((dir->stat.st_mode & VFS_MODE_DIRECTORY) == 0)
			{
				if (dir->close != NULL) dir->close(dir);
				kfree(dir);
				return NULL;
			};

			if ((!vfsCanCurrentThread(&dir->stat, 1)) && (flags & VFS_CHECK_ACCESS))
			{
				if (dir->close != NULL) dir->close(dir);
				kfree(dir);
				*error = VFS_PERM;
				return NULL;
			};

			Dir *subdir = (Dir*) kmalloc(sizeof(Dir));
			memset(subdir, 0, sizeof(Dir));

			if (dir->opendir(dir, subdir, sizeof(Dir)) != 0)
			{
				kfree(subdir);
				if (dir->close != NULL) dir->close(dir);
				kfree(dir);
				return NULL;
			};

			if (dir->close != NULL) dir->close(dir);
			kfree(dir);
			dir = subdir;

			scan++;		// skip over '/'
		}
		else
		{
			return dir;
		};
	};
};

int vfsStat(const char *path, struct stat *st)
{
	int error;
	Dir *dir = parsePath(path, VFS_CHECK_ACCESS, &error);
	if (dir == NULL)
	{
		return error;
	};

	memcpy(st, &dir->stat, sizeof(struct stat));
	if (dir->close != NULL) dir->close(dir);
	kfree(dir);
	return 0;
};

File *vfsOpen(const char *path, int flags, int *error)
{
	Dir *dir = parsePath(path, flags, error);
	if (dir == NULL)
	{
		return NULL;
	};

	if (dir->openfile == NULL)
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		return NULL;
	};

	File *file = (File*) kmalloc(sizeof(File));
	memset(file, 0, sizeof(File));

	if (dir->openfile(dir, file, sizeof(File)) != 0)
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		return NULL;
	};

	if (dir->close != NULL) dir->close(dir);
	kfree(dir);

	return file;
};

ssize_t vfsRead(File *file, void *buffer, size_t size)
{
	if (file->read == NULL) return -1;
	return file->read(file, buffer, size);
};

void vfsClose(File *file)
{
	if (file->close != NULL) file->close(file);
	kfree(file);
};
