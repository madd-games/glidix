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

#include <glidix/vfs.h>
#include <glidix/console.h>
#include <glidix/string.h>
#include <glidix/mount.h>
#include <glidix/memory.h>
#include <glidix/sched.h>
#include <glidix/spinlock.h>

static Spinlock vfsSpinlockCreation;

void vfsInit()
{
	spinlockRelease(&vfsSpinlockCreation);
};

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

char *realpath_from(const char *relpath, char *buffer, const char *fromdir)
{
	//Thread *ct = getCurrentThread();

	if (*relpath == 0)
	{
		*buffer = 0;
		return buffer;
	};

	char *put = buffer;
	size_t szput = 0;
	const char *scan = relpath;

	if (relpath[0] != '/')
	{
		strcpy(buffer, fromdir);
		szput = strlen(fromdir);
		put = &buffer[szput];
		if (*(put-1) != '/')
		{
			if (szput == 255)
			{
				return NULL;
			};

			*put++ = '/';
			szput++;
		};
	}
	else
	{
		*put++ = *scan++;			// put the slash there.
		szput++;
	};

	char token[128];
	char *tokput = token;
	size_t toksz = 0;

	while (1)
	{
		char c = *scan++;
		while ((c == '/') && ((*scan) == '/'))
		{
			c = *scan++;
		};
		
		if ((c != '/') && (c != 0))
		{
			if (toksz == 127)
			{
				return NULL;
			};

			*tokput++ = c;
			toksz++;
		}
		else
		{
			*tokput = 0;
			
			if (strcmp(token, ".") == 0)
			{
				if (szput != 1)
				{
					put--;
					szput--;
				};

				*put++ = c;
				szput++;
				if (c == 0)
				{
					break;
				}
				else
				{
					tokput = token;
					toksz = 0;
					continue;
				};
			};

			if (strcmp(token, "..") == 0)
			{
				if (szput == 1)
				{
					*put = 0;
					tokput = token;
					toksz = 0;
					continue;		// "parent directory" of root directory is root directory itself.
				};

				szput--;
				put--;
				*put = 0;

				while (*put != '/')
				{
					put--;
				};

				if (put != buffer) *put = 0;

				put++;
				*put = 0;
				szput = put - buffer;
				if (c == 0) break;
				else
				{
					tokput = token;
					toksz = 0;
					continue;
				};
			};

			if ((szput + toksz + 1) >= 255)
			{
				return NULL;
			};

			strcpy(put, token);
			put += toksz;
			*put++ = c;		// NUL or '/'

			szput += toksz + 1;
			if (c == 0) break;
			
			tokput = token;
			toksz = 0;
		};
	};

	if (strcmp(buffer, "//") == 0)
	{
		strcpy(buffer, "/");
	};

	return buffer;
};

char *realpath(const char *relpath, char *buffer)
{
	return realpath_from(relpath, buffer, getCurrentThread()->cwd);
};

Dir *resolvePath(const char *path, int flags, int *error, int level)
{
	if (level == VFS_MAX_LINK_DEPTH)
	{
		*error = VFS_LINK_LOOP;
		return NULL;
	};

	*error = VFS_NO_FILE;			// default error

	//kprintf_debug("start of parsePath('%s')\n", path);
	char rpath[256];
	if (realpath(path, rpath) == NULL)
	{
		return NULL;
	};
	//kprintf_debug("rpath='%s'\n", rpath);

	SplitPath spath;
	if (resolveMounts(rpath, &spath) != 0)
	{
		//kprintf_debug("could not resolve mounts\n");
		return NULL;
	};

	char token[128];
	char *end = (char*) &token[127];
	const char *scan = spath.filename;
	char currentDir[PATH_MAX];
	strcpy(currentDir, spath.parent);

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

	struct stat st_parent;
	st_parent.st_dev = 0;
	st_parent.st_ino = 2;
	st_parent.st_mode = 01755;
	st_parent.st_uid = 0;
	st_parent.st_gid = 0;

	while (1)
	{
		char *put = token;
		while ((*scan != 0) && (*scan != '/'))
		{
			if (put == end)
			{
				*put = 0;
				panic("parsePath(): token too long: '%s'\n", token);
			};
			*put++ = *scan++;
		};
		*put = 0;

		//kprintf_debug("token '%s'\n", token);
		if (strlen(token) == 0)
		{
			if (*scan == 0)
			{
				dir->stat.st_dev = spath.fs->dev;
				return dir;
			};

			if (dir->close != NULL) dir->close(dir);
			kfree(dir);
			return NULL;
		};

		while (strcmp(dir->dirent.d_name, token) != 0)
		{
			if (dir->next(dir) != 0)
			{
				if ((*scan == 0) && (flags & VFS_CREATE) && (token[0] != 0))
				{
					if (dir->mkreg != NULL)
					{
						//if (dir->getstat != NULL) dir->getstat(dir);

						if (st_parent.st_ino == 0) panic("parent with inode 0!");
						if (vfsCanCurrentThread(&st_parent, 2))
						{
							if (dir->mkreg(dir, token, (flags >> 3) & 0x0FFF,
									getCurrentThread()->euid, getCurrentThread()->egid) == 0)
							{
								
								dir->stat.st_dev = spath.fs->dev;
								return dir;
							};
						}
						else
						{
							*error = VFS_PERM;
						};
					};
				};

				if (dir->close != NULL) dir->close(dir);
				kfree(dir);
				return NULL;
			};
		};

		if (*scan == '/')
		{
			if (dir->getstat != NULL) dir->getstat(dir);

			if ((dir->stat.st_mode & 0xF000) != VFS_MODE_DIRECTORY)
			{
				if ((dir->stat.st_mode & 0xF000) == VFS_MODE_LINK)
				{
					//kprintf_debug("link?\n");
					char linkpath[PATH_MAX];
					dir->readlink(dir, linkpath);
					if (dir->close != NULL) dir->close(dir);
					kfree(dir);
					char target[PATH_MAX];
					if (realpath_from(linkpath, target, currentDir) == NULL)
					{
						*error = VFS_NO_FILE;
						return NULL;
					};

					char completePath[PATH_MAX];
					if ((strlen(target) + strlen(scan+1) + 2) > PATH_MAX)
					{
						*error = VFS_NO_FILE;
						return NULL;
					};

					strcpy(completePath, target);
					strcat(completePath, "/");
					strcat(completePath, scan+1);

					return resolvePath(completePath, flags, error, level+1);
				};

				*error = VFS_NOT_DIR;
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

			memcpy(&st_parent, &dir->stat, sizeof(struct stat));

			Dir *subdir = (Dir*) kmalloc(sizeof(Dir));
			memset(subdir, 0, sizeof(Dir));

			int derror;
			if ((derror = dir->opendir(dir, subdir, sizeof(Dir))) != 0)
			{
				if ((derror == VFS_EMPTY_DIRECTORY) && (flags & VFS_STOP_ON_EMPTY))
				{
					if (dir->close != NULL) dir->close(dir);
					kfree(dir);
					*error = VFS_EMPTY_DIRECTORY;
					return subdir;
				};

				if (dir->close != NULL) dir->close(dir);
				kfree(dir);

				if ((derror == VFS_EMPTY_DIRECTORY) && (flags & VFS_CREATE))
				{
					char *put = token;
					scan++;
					while (*scan != '/')
					{
						*put++ = *scan;
						if (*scan == 0) break;
						scan++;
					};

					*put = 0;
					//kprintf_debug("vfs: creating regular file '%s'\n", token);
					if (*scan == 0)
					{
						//kprintf_debug("ok scan is good\n");
						//if (dir->getstat != NULL) dir->getstat(dir);
						if (subdir->mkreg != NULL)
						{
							//kprintf_debug("ok subdir->mkreg is here\n");
							if (st_parent.st_ino == 0) panic("parent with inode 0!");
							if (vfsCanCurrentThread(&st_parent, 2))
							{
								if (subdir->mkreg(subdir, token, (flags >> 3) & 0x0FFF,
										getCurrentThread()->euid, getCurrentThread()->egid) == 0)
								{
									subdir->stat.st_dev = spath.fs->dev;
									return subdir;
								};
							}
							else
							{
								*error = VFS_PERM;
							};
						};
					};
				};

				if (subdir->close != NULL) subdir->close(subdir);
				kfree(subdir);
				*error = derror;
				return NULL;
			};

			strcat(currentDir, "/");
			strcat(currentDir, token);

			if (dir->close != NULL) dir->close(dir);
			kfree(dir);
			dir = subdir;

			scan++;		// skip over '/'
		}
		else
		{
			if (dir->getstat != NULL) dir->getstat(dir);

			if (((dir->stat.st_mode & 0xF000) == VFS_MODE_LINK) && ((flags & VFS_NO_FOLLOW) == 0))
			{
				// this is a link and we were not instructed to not-follow, so we follow.
				char linkpath[PATH_MAX];
				ssize_t rls = dir->readlink(dir, linkpath);
				if (dir->close != NULL) dir->close(dir);
				kfree(dir);
				if (rls == -1)
				{
					*error = VFS_IO_ERROR;
					return NULL;
				};

				char target[PATH_MAX];
				if (realpath_from(linkpath, target, currentDir) == NULL)
				{
					*error = VFS_NO_FILE;
					return NULL;
				};

				return resolvePath(target, flags, error, level+1);
			};

			dir->stat.st_dev = spath.fs->dev;
			return dir;
		};
	};
};

Dir *parsePath(const char *path, int flags, int *error)
{
	return resolvePath(path, flags, error, 0);
};

static int vfsStatGen(const char *path, struct stat *st, int flags)
{
	char rpath[256];
	if (realpath(path, rpath) == NULL)
	{
		return VFS_NO_FILE;
	};

	if (strcmp(rpath, "/") == 0)
	{
		// special case
		st->st_dev = 0;
		st->st_ino = 2;
		st->st_mode = 0755 | VFS_MODE_DIRECTORY | VFS_MODE_STICKY;
		st->st_nlink = 1;
		st->st_uid = 0;
		st->st_gid = 0;
		st->st_rdev = 0;
		st->st_size = 0;
		st->st_blksize = 512;
		st->st_blocks = 0;
		st->st_atime = 0;
		st->st_ctime = 0;
		st->st_mtime = 0;
		return 0;
	};

	if (path[strlen(path)-1] == '/')
	{
		return VFS_NO_FILE;
	};

	int error;
	Dir *dir = parsePath(path, VFS_CHECK_ACCESS | flags, &error);
	if (dir == NULL)
	{
		return error;
	};

	memcpy(st, &dir->stat, sizeof(struct stat));
	if (dir->close != NULL) dir->close(dir);
	kfree(dir);
	return 0;
};

int vfsStat(const char *path, struct stat *st)
{
	return vfsStatGen(path, st, 0);
};

int vfsLinkStat(const char *path, struct stat *st)
{
	return vfsStatGen(path, st, VFS_NO_FOLLOW);
};

File *vfsOpen(const char *path, int flags, int *error)
{
	if (path[strlen(path)-1] == '/')
	{
		*error = VFS_NO_FILE;
		return NULL;
	};

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

ssize_t vfsWrite(File *file, const void *buffer, size_t size)
{
	if (file->write == NULL) return -1;
	return file->write(file, buffer, size);
};

void vfsClose(File *file)
{
	if (file->close != NULL) file->close(file);
	kfree(file);
};

void vfsLockCreation()
{
	spinlockAcquire(&vfsSpinlockCreation);
};

void vfsUnlockCreation()
{
	spinlockRelease(&vfsSpinlockCreation);
};
