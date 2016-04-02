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

#include <glidix/syscall.h>
#include <glidix/sched.h>
#include <glidix/ftab.h>
#include <glidix/vfs.h>
#include <glidix/elf64.h>
#include <glidix/signal.h>
#include <glidix/memory.h>
#include <glidix/common.h>
#include <glidix/errno.h>
#include <glidix/string.h>
#include <glidix/procmem.h>
#include <glidix/module.h>
#include <glidix/console.h>
#include <glidix/fdopendir.h>
#include <glidix/fsdriver.h>
#include <glidix/time.h>
#include <glidix/pipe.h>
#include <glidix/shared.h>
#include <glidix/down.h>
#include <glidix/netif.h>
#include <glidix/socket.h>
#include <glidix/pci.h>
#include <glidix/utsname.h>
#include <glidix/thsync.h>
#include <glidix/message.h>
#include <glidix/shmem.h>

int isPointerValid(uint64_t ptr, uint64_t size, int flags)
{
	if (size == 0) return 1;

	if (ptr < 0x1000)
	{
		return 0;
	};

	if ((ptr+size) > 0x7FC0000000)
	{
		return 0;
	};

	uint64_t start = ptr / 0x1000;
	uint64_t count = size / 0x1000;
	if (size % 0x1000) count++;
	
	uint64_t i;
	for (i=start; i<start+count; i++)
	{
		if (!canAccessPage(getCurrentThread()->pm, i, flags))
		{
			return 0;
		};
	};
	
	return 1;
};

int isStringValid(uint64_t ptr)
{
	// TODO
	return 1;
};

void sys_exit(int status)
{
	threadExit(getCurrentThread(), status);
	kyield();
};

uint64_t sys_write(int fd, const void *buf, size_t size)
{
	if (!isPointerValid((uint64_t)buf, (uint64_t)size, PROT_READ))
	{
		ERRNO = EFAULT;
		ssize_t err = -1;
		return *((uint64_t*)&err);
	};
	
	ssize_t out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			if (fp->write == NULL)
			{
				spinlockRelease(&ftab->spinlock);
				getCurrentThread()->therrno = EROFS;
				out = -1;
			}
			else
			{
				if ((fp->oflag & O_WRONLY) == 0)
				{
					spinlockRelease(&ftab->spinlock);
					getCurrentThread()->therrno = EPERM;
					out = -1;
				}
				else
				{
					vfsDup(fp);
					spinlockRelease(&ftab->spinlock);
					out = fp->write(fp, buf, size);
					vfsClose(fp);
				};
			};
		};
	};

	return *((uint64_t*)&out);
};

ssize_t sys_read(int fd, void *buf, size_t size)
{
	if (!isPointerValid((uint64_t)buf, (uint64_t)size, PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	ssize_t out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			if (fp->read == NULL)
			{
				spinlockRelease(&ftab->spinlock);
				getCurrentThread()->therrno = EIO;
				out = -1;
			}
			else
			{
				if ((fp->oflag & O_RDONLY) == 0)
				{
					spinlockRelease(&ftab->spinlock);
					getCurrentThread()->therrno = EPERM;
					out = -1;
				}
				else
				{
					vfsDup(fp);
					spinlockRelease(&ftab->spinlock);
					out = fp->read(fp, buf, size);
					vfsClose(fp);
				};
			};
		};
	};

	return out;
};

int sysOpenErrno(int vfsError)
{
	switch (vfsError)
	{
	case VFS_PERM:
		getCurrentThread()->therrno = EACCES;
		break;
	case VFS_NO_FILE:
		getCurrentThread()->therrno = ENOENT;
		break;
	case VFS_FILE_LIMIT_EXCEEDED:
		getCurrentThread()->therrno = EMFILE;
		break;
	case VFS_BUSY:
		getCurrentThread()->therrno = EBUSY;
		break;
	case VFS_NOT_DIR:
		getCurrentThread()->therrno = ENOTDIR;
		break;
	case VFS_LINK_LOOP:
		getCurrentThread()->therrno = ELOOP;
		break;
	default:
		/* fallback in case there are some unhandled errors */
		getCurrentThread()->therrno = EIO;
		break;
	};
	return -1;
};

int sys_open(const char *path, int oflag, mode_t mode)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	mode &= 0x0FFF;
	mode &= ~(getCurrentThread()->umask);

	if ((oflag & O_ALL) != oflag)
	{
		/* unrecognised bits were set */
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};

	vfsLockCreation();

	struct stat st;
	int error = vfsStat(path, &st);
	if (error != 0)
	{
		if (((error != VFS_NO_FILE) && (error != VFS_EMPTY_DIRECTORY)) || ((oflag & O_CREAT) == 0))
		{
			vfsUnlockCreation();
			return sysOpenErrno(error);
		}
		else
		{
			// we're creating a new file, fake a stat.
			st.st_uid = getCurrentThread()->euid;
			st.st_gid = getCurrentThread()->egid;
			st.st_mode = 0777;				// because mode shall not affect the open (see POSIX open() )
		};
	}
	else if (oflag & O_EXCL)
	{
		// file exists
		vfsUnlockCreation();
		getCurrentThread()->therrno = EEXIST;
		return -1;
	};

	if ((st.st_mode & 0xF000) == 0x1000)
	{
		// directory, do not open.
		vfsUnlockCreation();
		getCurrentThread()->therrno = EISDIR;
		return -1;
	};

	int neededPerms = 0;
	if (oflag & O_RDONLY)
	{
		neededPerms |= 4;
	};
	if (oflag & O_WRONLY)
	{
		neededPerms |= 2;
	};

	if (!vfsCanCurrentThread(&st, neededPerms))
	{
		if ((oflag & O_CREAT) == 0)
		{
			vfsUnlockCreation();
			return sysOpenErrno(VFS_PERM);
		};
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	int i;
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		if (ftab->entries[i] == NULL)
		{
			break;
		};
	};

	if (i == MAX_OPEN_FILES)
	{
		vfsUnlockCreation();
		spinlockRelease(&ftab->spinlock);
		return sysOpenErrno(VFS_FILE_LIMIT_EXCEEDED);
	};

	int flags = VFS_CHECK_ACCESS;
	if (oflag & O_CREAT)
	{
		flags |= VFS_CREATE | (mode << 3);
	};

	File *fp = vfsOpen(path, flags, &error);
	if (fp == NULL)
	{
		vfsUnlockCreation();
		spinlockRelease(&ftab->spinlock);
		return sysOpenErrno(error);
	};

	vfsUnlockCreation();

	if (oflag & O_TRUNC)
	{
		if (fp->truncate != NULL) fp->truncate(fp, 0);
	};

	if (oflag & O_APPEND)
	{
		if (fp->seek != NULL) fp->seek(fp, 0, SEEK_END);
	};

	fp->oflag = oflag;
	fp->refcount = 1;
	
	ftab->entries[i] = fp;
	spinlockRelease(&ftab->spinlock);

	return i;
};

int sys_close(int fd)
{
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		return 0;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	if (ftab->entries[fd] != NULL)
	{
		File *fp = ftab->entries[fd];
		ftab->entries[fd] = NULL;
		spinlockRelease(&ftab->spinlock);
		vfsClose(fp);
		return 0;
	};

	spinlockRelease(&ftab->spinlock);
	return 0;
};

int sys_ioctl(int fd, uint64_t cmd, void *argp)
{
	if (!isPointerValid((uint64_t)argp, (cmd >> 32) & 0xFFFF, PROT_READ | PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};

	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	if (ftab->entries[fd] == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	File *fp = ftab->entries[fd];
	if (fp->ioctl == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	vfsDup(fp);
	spinlockRelease(&ftab->spinlock);
	int ret = fp->ioctl(fp, cmd, argp);
	vfsClose(fp);
	return ret;
};

off_t sys_lseek(int fd, off_t offset, int whence)
{
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		return (off_t)-1;
	};

	if ((whence != SEEK_SET) && (whence != SEEK_CUR) && (whence != SEEK_END))
	{
		getCurrentThread()->therrno = EINVAL;
		return (off_t)-1;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	File *fp = ftab->entries[fd];
	if (fp == NULL)
	{
		getCurrentThread()->therrno = EBADF;
		spinlockRelease(&ftab->spinlock);
		return (off_t)-1;
	};

	if (fp->seek == NULL)
	{
		getCurrentThread()->therrno = ESPIPE;
		spinlockRelease(&ftab->spinlock);
		return (off_t)-1;
	};

	vfsDup(fp);
	spinlockRelease(&ftab->spinlock);
	
	off_t ret = fp->seek(fp, offset, whence);
	if (ret == (off_t)-1)
	{
		getCurrentThread()->therrno = EOVERFLOW;
	};

	vfsClose(fp);
	return ret;
};

int sys_raise(int sig)
{
	if ((sig < 0) || (sig >= SIG_NUM))
	{
		ERRNO = EINVAL;
		return -1;
	};

	siginfo_t siginfo;
	siginfo.si_signo = sig;
	siginfo.si_code = 0;

	cli();
	lockSched();
	sendSignal(getCurrentThread(), &siginfo);
	unlockSched();
	sti();

	return 0;
};

int sys_stat(const char *path, struct stat *buf)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isPointerValid((uint64_t)buf, sizeof(struct stat), PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};

	int status = vfsStat(path, buf);
	if (status == 0)
	{
		return status;
	}
	else
	{
		return sysOpenErrno(status);
	};
};

int sys_lstat(const char *path, struct stat *buf)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isPointerValid((uint64_t)buf, sizeof(struct stat), PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int status = vfsLinkStat(path, buf);
	if (status == 0)
	{
		return status;
	}
	else
	{
		return sysOpenErrno(status);
	};
};

int sys_pause()
{
	ASM("cli");
	lockSched();
	getCurrentThread()->therrno = EINTR;
	//getCurrentThread()->flags |= THREAD_WAITING;
	waitThread(getCurrentThread());
	unlockSched();
	kyield();
	return -1;
};

int sys_fstat(int fd, struct stat *buf)
{
	if (!isPointerValid((uint64_t)buf, sizeof(struct stat), PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	File *fp = ftab->entries[fd];
	if (fp == NULL)
	{
		getCurrentThread()->therrno = EBADF;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	if (fp->fstat == NULL)
	{
		getCurrentThread()->therrno = EIO;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	vfsDup(fp);
	spinlockRelease(&ftab->spinlock);
	int status = fp->fstat(fp, buf);
	vfsClose(fp);
	
	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
	};

	return status;
};

int sys_chmod(const char *path, mode_t mode)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int error;
	Dir *dir = parsePath(path, VFS_CHECK_ACCESS, &error);

	if (dir == NULL)
	{
		return sysOpenErrno(error);
	};

	if (dir->chmod == NULL)
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	if ((getCurrentThread()->euid != 0) && (getCurrentThread()->euid != dir->stat.st_uid))
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		getCurrentThread()->therrno = EPERM;
		return -1;
	};

	int status = dir->chmod(dir, mode);
	if (dir->close != NULL) dir->close(dir);
	kfree(dir);

	if (status != 0) getCurrentThread()->therrno = EIO;
	return status;
};

int sys_fchmod(int fd, mode_t mode)
{
	// fstat first.
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	File *fp = ftab->entries[fd];
	if (fp == NULL)
	{
		getCurrentThread()->therrno = EBADF;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	if (fp->fstat == NULL)
	{
		getCurrentThread()->therrno = EIO;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	if (fp->fchmod == NULL)
	{
		getCurrentThread()->therrno = EIO;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	struct stat st;
	vfsDup(fp);
	spinlockRelease(&ftab->spinlock);
	int status = fp->fstat(fp, &st);
	
	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
		vfsClose(fp);
		return -1;
	};

	if ((getCurrentThread()->euid != 0) && (getCurrentThread()->euid != st.st_uid))
	{
		getCurrentThread()->therrno = EPERM;
		vfsClose(fp);
		return -1;
	};

	status = fp->fchmod(fp, mode);
	vfsClose(fp);
	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	return 0;
};

static int sys_fsync(int fd)
{
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	File *fp = ftab->entries[fd];
	if (fp == NULL)
	{
		getCurrentThread()->therrno = EBADF;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	vfsDup(fp);
	spinlockRelease(&ftab->spinlock);
	if (fp->fsync != NULL)
	{
		fp->fsync(fp);
	};
	vfsClose(fp);

	return 0;
};

static int canChangeOwner(struct stat *st, uid_t uid, gid_t gid)
{
	Thread *ct = getCurrentThread();
	if (ct->euid == 0) return 1;

	if ((ct->euid == uid) && (uid == st->st_uid))
	{
		return (ct->egid == gid) || (ct->sgid == gid) || (ct->rgid == gid);
	};

	return 0;
};

int sys_chown(const char *path, uid_t uid, gid_t gid)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if ((uid == (uid_t)-1) && (gid == (gid_t)-1)) return 0;

	int error;
	Dir *dir = parsePath(path, VFS_CHECK_ACCESS, &error);

	if (dir == NULL)
	{
		return sysOpenErrno(error);
	};

	if (dir->chown == NULL)
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	if (uid == (uid_t)-1)
	{
		uid = dir->stat.st_uid;
	};

	if (gid == (gid_t)-1)
	{
		gid = dir->stat.st_gid;
	};

	if (!canChangeOwner(&dir->stat, uid, gid))
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		getCurrentThread()->therrno = EPERM;
		return -1;
	};

	int status = dir->chown(dir, uid, gid);
	if (dir->close != NULL) dir->close(dir);
	kfree(dir);

	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
	};

	return status;
};

int sys_fchown(int fd, uid_t uid, gid_t gid)
{
	// fstat first.
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	File *fp = ftab->entries[fd];
	if (fp == NULL)
	{
		getCurrentThread()->therrno = EBADF;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	if (fp->fstat == NULL)
	{
		getCurrentThread()->therrno = EIO;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	if (fp->fchown == NULL)
	{
		getCurrentThread()->therrno = EIO;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	struct stat st;
	vfsDup(fp);
	spinlockRelease(&ftab->spinlock);
	int status = fp->fstat(fp, &st);

	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
		vfsClose(fp);
		return -1;
	};

	if (!canChangeOwner(&st, uid, gid))
	{
		getCurrentThread()->therrno = EPERM;
		vfsClose(fp);
		return -1;
	};

	status = fp->fchown(fp, uid, gid);
	vfsClose(fp);
	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	return 0;
};

int sys_mkdir(const char *path, mode_t mode)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	mode &= 0xFFF;
	mode &= ~(getCurrentThread()->umask);

	char rpath[256];
	if (realpath(path, rpath) == NULL)
	{
		getCurrentThread()->therrno = ENOENT;
		return -1;
	};

	char parent[256];
	char newdir[256];

	size_t sz = strlen(rpath);
	while (rpath[sz] != '/')
	{
		sz--;
	};

	memcpy(parent, rpath, sz);
	parent[sz] = 0;
	if (parent[0] == 0)
	{
		strcpy(parent, "/");
	};

	strcpy(newdir, &rpath[sz+1]);
	if (strlen(newdir) >= 128)
	{
		getCurrentThread()->therrno = ENAMETOOLONG;
		return -1;
	};

	if (newdir[0] == 0)
	{
		getCurrentThread()->therrno = ENOENT;
		return -1;
	};

	vfsLockCreation();

	struct stat st;
	int error;
	if ((error = vfsStat(parent, &st)) != 0)
	{
		vfsUnlockCreation();
		return sysOpenErrno(error);
	};

	if (!vfsCanCurrentThread(&st, 2))
	{
		vfsUnlockCreation();
		getCurrentThread()->therrno = EPERM;
		return -1;
	};

	if (strcmp(parent, "/") != 0) strcat(parent, "/");

	Dir *dir = parsePath(parent, VFS_STOP_ON_EMPTY, &error);
	int endYet = (error == VFS_EMPTY_DIRECTORY);
	if (dir->mkdir == NULL)
	{
		vfsUnlockCreation();
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	while (!endYet)
	{
		if (strcmp(dir->dirent.d_name, newdir) == 0)
		{
			if (dir->close != NULL) dir->close(dir);
			kfree(dir);
			vfsUnlockCreation();
			getCurrentThread()->therrno = EEXIST;
			return -1;
		};

		if (dir->next(dir) == -1)
		{
			endYet = 1;
		};
	};

	int status = dir->mkdir(dir, newdir, mode & 0x0FFF, getCurrentThread()->euid, getCurrentThread()->egid);

	if (dir->close != NULL) dir->close(dir);
	kfree(dir);
	vfsUnlockCreation();

	if (status != 0)
	{
		getCurrentThread()->therrno = EIO;
	};

	return status;
};

int sys_ftruncate(int fd, off_t length)
{
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	File *fp = ftab->entries[fd];
	if (fp == NULL)
	{
		getCurrentThread()->therrno = EBADF;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	if ((fp->oflag & O_WRONLY) == 0)
	{
		getCurrentThread()->therrno = EPERM;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	if (fp->truncate == NULL)
	{
		getCurrentThread()->therrno = EIO;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	vfsDup(fp);
	spinlockRelease(&ftab->spinlock);
	fp->truncate(fp, length);
	vfsClose(fp);
	
	return 0;
};

int sys_unlink(const char *path)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	char rpath[256];
	if (realpath(path, rpath) == NULL)
	{
		getCurrentThread()->therrno = ENOENT;
		return -1;
	};

	char parent[256];
	char child[256];

	size_t sz = strlen(rpath);
	while (rpath[sz] != '/')
	{
		sz--;
	};

	memcpy(parent, rpath, sz);
	parent[sz] = 0;
	if (parent[0] == 0)
	{
		strcpy(parent, "/");
	};

	strcpy(child, &rpath[sz+1]);
	if (strlen(child) >= 128)
	{
		getCurrentThread()->therrno = ENOENT;
		return -1;
	};

	if (child[0] == 0)
	{
		getCurrentThread()->therrno = ENOENT;
		return -1;
	};

	vfsLockCreation();

	struct stat st;
	int error;
	if ((error = vfsStat(parent, &st)) != 0)
	{
		vfsUnlockCreation();
		return sysOpenErrno(error);
	};

	Dir *dir = parsePath(rpath, VFS_NO_FOLLOW, &error);
	if (dir == NULL)
	{
		vfsUnlockCreation();
		return sysOpenErrno(error);
	};

	if (((dir->stat.st_mode & 0xF000) == 0x1000) && (dir->stat.st_size != 0))
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		vfsUnlockCreation();
		getCurrentThread()->therrno = ENOTEMPTY;
		return -1;
	};

	if ((st.st_mode & VFS_MODE_STICKY))
	{
		uid_t uid = getCurrentThread()->euid;
		if ((st.st_uid != uid) && (dir->stat.st_uid != uid))
		{
			if (dir->close != NULL) dir->close(dir);
			kfree(dir);
			vfsUnlockCreation();
			getCurrentThread()->therrno = EPERM;
			return -1;
		};
	};

	if (!vfsCanCurrentThread(&st, 2))
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		vfsUnlockCreation();
		getCurrentThread()->therrno = EPERM;
		return -1;
	};

	if (dir->unlink == NULL)
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		vfsUnlockCreation();
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	if (dir->unlink(dir) != 0)
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		vfsUnlockCreation();
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	if (dir->close != NULL) dir->close(dir);
	kfree(dir);

	vfsUnlockCreation();
	return 0;
};

int sys_dup(int fd)
{
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	File *fp = ftab->entries[fd];
	if (fp == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	int i;
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		if (ftab->entries[i] == NULL)
		{
			break;
		};
	};

	if (i == MAX_OPEN_FILES)
	{
		spinlockRelease(&ftab->spinlock);
		return sysOpenErrno(VFS_FILE_LIMIT_EXCEEDED);
	};

	vfsDup(fp);
	ftab->entries[i] = fp;
	spinlockRelease(&ftab->spinlock);

	return i;
};

int sys_dup2(int oldfd, int newfd)
{
	if ((oldfd < 0) || (oldfd >= MAX_OPEN_FILES))
	{
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	if ((newfd < 0) || (newfd >= MAX_OPEN_FILES))
	{
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	if (newfd == oldfd)
	{
		return newfd;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	File *fp = ftab->entries[oldfd];
	if (fp == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = EBADF;
		return -1;
	};

	if (ftab->entries[newfd] != NULL)
	{
		File *toclose = ftab->entries[newfd];
		vfsClose(toclose);
	};
	vfsDup(fp);
	ftab->entries[newfd] = fp;
	spinlockRelease(&ftab->spinlock);

	return newfd;
};

int sys_insmod(const char *modname, const char *path, const char *opt, int flags)
{
	if (!isStringValid((uint64_t)modname))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (opt != NULL)
	{
		if (!isStringValid((uint64_t)opt))
		{
			ERRNO = EFAULT;
			return -1;
		};
	};
	
	if (getCurrentThread()->euid != 0)
	{
		// only root can load modules.
		getCurrentThread()->therrno = EPERM;
		return -1;
	};

	return insmod(modname, path, opt, flags);
};

int sys_chdir(const char *path)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	struct stat st;
	int error = vfsStat(path, &st);
	if (error != 0)
	{
		return sysOpenErrno(error);
	};

	char rpath[256];
	if (realpath(path, rpath) == NULL)
	{
		getCurrentThread()->therrno = EACCES;
		return -1;
	};

	if ((st.st_mode & VFS_MODE_DIRECTORY) == 0)
	{
		getCurrentThread()->therrno = ENOTDIR;
		return -1;
	};

	if (!vfsCanCurrentThread(&st, 1))
	{
		getCurrentThread()->therrno = EACCES;
		return -1;
	};

	strcpy(getCurrentThread()->cwd, rpath);
	return 0;
};

char *sys_getcwd(char *buf, size_t size)
{
	if (!isPointerValid((uint64_t)buf, size, PROT_WRITE))
	{
		ERRNO = EFAULT;
		return NULL;
	};
	
	if (size > 256) size = 256;
	memcpy(buf, getCurrentThread()->cwd, size);
	return buf;
};

/**
 * Userspace mmap() should wrap around this. Returns the address of the END of the mapped region,
 * and 'addr' must be a valid address where the specified region can fit without segment collisions.
 */
uint64_t sys_mmap(uint64_t addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	int allProt = PROT_READ | PROT_WRITE | PROT_EXEC;
	int allFlags = MAP_PRIVATE | MAP_SHARED;

	if (prot == 0)
	{
		getCurrentThread()->therrno = EINVAL;
		return 0;
	};

	if ((prot & allProt) != prot)
	{
		getCurrentThread()->therrno = EINVAL;
		return 0;
	};

	if ((flags & allFlags) != flags)
	{
		getCurrentThread()->therrno = EINVAL;
		return 0;
	};

	if ((flags & (MAP_PRIVATE | MAP_SHARED)) == 0)
	{
		getCurrentThread()->therrno = EINVAL;
		return 0;
	};

	if ((flags & MAP_PRIVATE) && (flags & MAP_SHARED))
	{
		getCurrentThread()->therrno = EINVAL;
		return 0;
	};

	if (len == 0)
	{
		getCurrentThread()->therrno = EINVAL;
		return 0;
	};

	addr &= ~0xFFF;
	if ((addr < 0x1000) || ((addr+len) > 0x7FC0000000))
	{
		getCurrentThread()->therrno = EINVAL;
		return 0;
	};

	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		getCurrentThread()->therrno = EBADF;
		return 0;
	};

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	File *fp = ftab->entries[fd];
	if (fp->mmap == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = ENODEV;
		return 0;
	};

	int neededPerms = 0;
	if (prot & PROT_READ)
	{
		neededPerms |= O_RDONLY;
	};

	if (prot & PROT_WRITE)
	{
		// PROT_WRITE implies PROT_READ.
		neededPerms |= O_RDWR;
	};

	if (prot & PROT_EXEC)
	{
		// PROT_EXEC implies PROT_READ
		neededPerms |= O_RDONLY;
	};

	if ((fp->oflag & neededPerms) != neededPerms)
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = EACCES;
		return 0;
	};

	vfsDup(fp);
	spinlockRelease(&ftab->spinlock);
	FrameList *fl = fp->mmap(fp, len, prot, flags, offset);
	vfsClose(fp);
	if (fl == NULL)
	{
		getCurrentThread()->therrno = ENODEV;
		return 0;
	};

	if (AddSegment(getCurrentThread()->pm, addr/0x1000, fl, prot) != 0)
	{
		pdownref(fl);
		getCurrentThread()->therrno = EINVAL;
		return 0;
	};

	uint64_t out = addr + 0x1000 * fl->count;
	pdownref(fl);

	return out;
};

int setuid(uid_t uid)
{
	Thread *me = getCurrentThread();
	if ((me->euid != 0) && (uid != me->euid) && (uid != me->ruid))
	{
		me->therrno = EPERM;
		return -1;
	};

	if (me->euid == 0)
	{
		me->ruid = uid;
		me->suid = uid;
	};

	me->euid = uid;
	return 0;
};

int setgid(gid_t gid)
{
	Thread *me = getCurrentThread();
	if ((me->egid != 0) && (gid != me->egid) && (gid != me->rgid))
	{
		me->therrno = EPERM;
		return -1;
	};

	if (me->egid == 0)
	{
		me->rgid = gid;
		me->sgid = gid;
	};

	me->egid = gid;
	return 0;
};

int setreuid(uid_t ruid, uid_t euid)
{
	Thread *me = getCurrentThread();
	uid_t ruidPrev = me->ruid;
	if (ruid != (uid_t)-1)
	{
		if (me->euid != 0)
		{
			me->therrno = EPERM;
			return -1;
		};

		me->ruid = ruid;
	};

	if (euid != (uid_t)-1)
	{
		if (euid != ruidPrev)
		{
			if (me->euid != 0)
			{
				me->therrno = EPERM;
				return -1;
			};

			me->suid = euid;
		};

		me->euid = euid;
	};

	return 0;
};

int setregid(gid_t rgid, gid_t egid)
{
	Thread *me = getCurrentThread();
	gid_t rgidPrev = me->rgid;
	if (rgid != (gid_t)-1)
	{
		if (me->egid != 0)
		{
			me->therrno = EPERM;
			return -1;
		};

		me->rgid = rgid;
	};

	if (egid != (gid_t)-1)
	{
		if (egid != rgidPrev)
		{
			if (me->egid != 0)
			{
				me->therrno = EPERM;
				return -1;
			};

			me->sgid = egid;
		};

		me->egid = egid;
	};

	return 0;
};

int seteuid(uid_t euid)
{
	Thread *me = getCurrentThread();
	if (me->euid != 0)
	{
		if ((euid != me->euid) && (euid != me->ruid) && (euid != me->suid))
		{
			me->therrno = EPERM;
			return -1;
		};
	};

	me->euid = euid;
	return 0;
};

int setegid(gid_t egid)
{
	Thread *me = getCurrentThread();
	if (me->egid != 0)
	{
		if ((egid != me->egid) && (egid != me->rgid) && (egid != me->sgid))
		{
			me->therrno = EPERM;
			return -1;
		};
	};

	me->egid = egid;
	return 0;
};

int sys_rmmod(const char *modname, int flags)
{
	if (!isStringValid((uint64_t)modname))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	// only root can remove modules!
	if (getCurrentThread()->euid != 0)
	{
		getCurrentThread()->therrno = EPERM;
		return -1;
	};

	return rmmod(modname, flags);
};

int sys_link(const char *oldname, const char *newname)
{
	if (!isStringValid((uint64_t)oldname))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isStringValid((uint64_t)newname))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	char rpath[256];
	if (realpath(newname, rpath) == NULL)
	{
		getCurrentThread()->therrno = ENOENT;
		return -1;
	};

	char parent[256];
	char child[256];

	size_t sz = strlen(rpath);
	while (rpath[sz] != '/')
	{
		sz--;
	};

	memcpy(parent, rpath, sz);
	parent[sz] = 0;
	if (parent[0] == 0)
	{
		strcpy(parent, "/");
	};

	strcpy(child, &rpath[sz+1]);
	if (strlen(child) >= 128)
	{
		getCurrentThread()->therrno = ENOENT;
		return -1;
	};

	if (child[0] == 0)
	{
		getCurrentThread()->therrno = ENOENT;
		return -1;
	};

	vfsLockCreation();

	struct stat stdir;
	int error;
	if ((error = vfsStat(parent, &stdir)) != 0)
	{
		vfsUnlockCreation();
		return sysOpenErrno(error);
	};

	struct stat stold;
	if ((error = vfsLinkStat(oldname, &stold)) != 0)
	{
		vfsUnlockCreation();
		return sysOpenErrno(error);
	};

	if ((stold.st_mode & 0xF000) == VFS_MODE_DIRECTORY)
	{
		vfsUnlockCreation();
		getCurrentThread()->therrno = EPERM;
		return -1;
	};

	if (stold.st_dev != stdir.st_dev)
	{
		vfsUnlockCreation();
		getCurrentThread()->therrno = EXDEV;
		return -1;
	};

	// make sure we can write to the directory
	if (!vfsCanCurrentThread(&stold, 2))
	{
		vfsUnlockCreation();
		getCurrentThread()->therrno = EACCES;
		return -1;
	};

	if (parent[strlen(parent-1)] != '/')
	{
		strcat(parent, "/");
	};

	if (strcmp(parent, "/") != 0) strcat(parent, "/");

	error = 0;
	Dir *dir = parsePath(parent, VFS_STOP_ON_EMPTY, &error);
	if (dir == NULL)
	{
		vfsUnlockCreation();
		return sysOpenErrno(error);
	};

	if (error != VFS_EMPTY_DIRECTORY)
	{
		do
		{
			if (strcmp(dir->dirent.d_name, child) == 0)
			{
				if (dir->close != NULL) dir->close(dir);
				kfree(dir);
				vfsUnlockCreation();
				getCurrentThread()->therrno = EEXIST;
				return -1;
			};
		} while (dir->next(dir) != -1);
	};

	if (dir->link == NULL)
	{
		vfsUnlockCreation();
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		getCurrentThread()->therrno = EMLINK;
		return -1;
	};

	int status = dir->link(dir, child, stold.st_ino);

	vfsUnlockCreation();
	if (dir->close != NULL) dir->close(dir);
	kfree(dir);

	if (status != 0)
	{
		getCurrentThread()->therrno = EIO;
	};

	return status;
};

int sys_symlink(const char *target, const char *path)
{
	if (!isStringValid((uint64_t)target))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (strlen(target) > PATH_MAX)
	{
		getCurrentThread()->therrno = ENAMETOOLONG;
		return -1;
	};

	char rpath[256];
	if (realpath(path, rpath) == NULL)
	{
		getCurrentThread()->therrno = ENOENT;
		return -1;
	};

	char parent[256];
	char newlink[256];

	size_t sz = strlen(rpath);
	while (rpath[sz] != '/')
	{
		sz--;
	};

	memcpy(parent, rpath, sz);
	parent[sz] = 0;
	if (parent[0] == 0)
	{
		strcpy(parent, "/");
	};

	strcpy(newlink, &rpath[sz+1]);
	if (strlen(newlink) >= 128)
	{
		getCurrentThread()->therrno = ENAMETOOLONG;
		return -1;
	};

	if (newlink[0] == 0)
	{
		getCurrentThread()->therrno = ENOENT;
		return -1;
	};

	vfsLockCreation();

	struct stat st;
	int error;
	if ((error = vfsStat(parent, &st)) != 0)
	{
		vfsUnlockCreation();
		return sysOpenErrno(error);
	};

	if (!vfsCanCurrentThread(&st, 2))
	{
		vfsUnlockCreation();
		getCurrentThread()->therrno = EPERM;
		return -1;
	};

	if (strcmp(parent, "/") != 0) strcat(parent, "/");

	Dir *dir = parsePath(parent, VFS_STOP_ON_EMPTY, &error);
	int endYet = (error == VFS_EMPTY_DIRECTORY);
	if (dir->mkdir == NULL)
	{
		vfsUnlockCreation();
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	while (!endYet)
	{
		if (strcmp(dir->dirent.d_name, newlink) == 0)
		{
			if (dir->close != NULL) dir->close(dir);
			kfree(dir);
			vfsUnlockCreation();
			getCurrentThread()->therrno = EEXIST;
			return -1;
		};

		if (dir->next(dir) == -1)
		{
			endYet = 1;
		};
	};

	int status = dir->symlink(dir, newlink, target);

	if (dir->close != NULL) dir->close(dir);
	kfree(dir);
	vfsUnlockCreation();

	if (status != 0)
	{
		getCurrentThread()->therrno = EIO;
	};

	return status;
};

ssize_t sys_readlink(const char *path, char *buf, size_t bufsize)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isPointerValid((uint64_t)buf, bufsize, PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	char tmpbuf[PATH_MAX];
	int error;
	Dir *dir = parsePath(path, VFS_NO_FOLLOW | VFS_CHECK_ACCESS, &error);
	if (dir == NULL)
	{
		return sysOpenErrno(error);
	};

	if ((dir->stat.st_mode & 0xF000) != VFS_MODE_LINK)
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};

	ssize_t out = dir->readlink(dir, tmpbuf);
	if (dir->close != NULL) dir->close(dir);
	kfree(dir);

	if (out == -1)
	{
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	if (bufsize > out)
	{
		bufsize = out;
	};

	if (bufsize > PATH_MAX)
	{
		getCurrentThread()->therrno = ENAMETOOLONG;
		return -1;
	};

	memcpy(buf, tmpbuf, bufsize);
	return out;
};

unsigned sys_sleep(unsigned seconds)
{
	if (seconds == 0) return 0;

	ASM("cli");
	uint64_t sleepStart = (uint64_t) getTicks();
	uint64_t wakeTime = ((uint64_t) getTicks() + (uint64_t) seconds * (uint64_t) 1000);
	getCurrentThread()->wakeTime = wakeTime;
	//getCurrentThread()->flags |= THREAD_WAITING;
	waitThread(getCurrentThread());
	kyield();

	uint64_t timeDelta = (uint64_t) getTicks() - sleepStart;
	unsigned secondsSlept = (unsigned) timeDelta / 1000;

	if (secondsSlept >= seconds)
	{
		return 0;
	}
	else
	{
		return seconds-secondsSlept;
	};
};

int sys_utime(const char *path, time_t atime, time_t mtime)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int error;
	Dir *dirp = parsePath(path, VFS_CHECK_ACCESS, &error);

	if (dirp == NULL)
	{
		return sysOpenErrno(error);
	};

	if ((atime == 0) && (mtime == 0))
	{
		if (!vfsCanCurrentThread(&dirp->stat, 2))
		{
			getCurrentThread()->therrno = EACCES;
			return -1;
		};

		atime = time();
		mtime = atime;
	}
	else
	{
		if (dirp->stat.st_uid != getCurrentThread()->euid)
		{
			getCurrentThread()->therrno = EACCES;
			return -1;
		};
	};

	if (dirp->utime == NULL)
	{
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	if (dirp->utime(dirp, atime, mtime) != 0)
	{
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	return 0;
};

mode_t sys_umask(mode_t cmask)
{
	mode_t old = getCurrentThread()->umask;
	getCurrentThread()->umask = (cmask & 0777);
	return old;
};

int sys_socket(int domain, int type, int proto)
{
	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	int i;
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		if (ftab->entries[i] == NULL)
		{
			break;
		};
	};

	if (i == MAX_OPEN_FILES)
	{
		spinlockRelease(&ftab->spinlock);
		return sysOpenErrno(VFS_FILE_LIMIT_EXCEEDED);
	};
	
	File *fp = CreateSocket(domain, type, proto);
	if (fp == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		// errno set by CreateSocket()
		return -1;
	};
	
	fp->refcount = 1;
	ftab->entries[i] = fp;
	spinlockRelease(&ftab->spinlock);
	return i;
};

int sys_bind(int fd, const struct sockaddr *addr, size_t addrlen)
{
	if (!isPointerValid((uint64_t)addr, addrlen, PROT_READ))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			out = BindSocket(fp, addr, addrlen);
			vfsClose(fp);
		};
	};

	return out;
};

ssize_t sys_sendto(int fd, const void *message, size_t len, int flags, const struct sockaddr *addr, size_t addrlen)
{
	if (!isPointerValid((uint64_t)message, len, PROT_READ))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isPointerValid((uint64_t)addr, addrlen, PROT_READ))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			out = SendtoSocket(fp, message, len, flags, addr, addrlen);
			vfsClose(fp);
		};
	};

	return out;
};

ssize_t sys_recvfrom(int fd, void *message, size_t len, int flags, struct sockaddr *addr, size_t *addrlen)
{
	if (!isPointerValid((uint64_t)message, len, PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if ((addr != NULL) || (addrlen != NULL))
	{
		if (!isPointerValid((uint64_t)addrlen, 8, PROT_READ | PROT_WRITE))
		{
			ERRNO = EFAULT;
			return -1;
		};
		
		if (!isPointerValid((uint64_t)addr, *addrlen, PROT_READ | PROT_WRITE))
		{
			ERRNO = EFAULT;
			return -1;
		};
	};
	
	ssize_t out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			out = RecvfromSocket(fp, message, len, flags, addr, addrlen);
			vfsClose(fp);
		};
	};

	return out;
};

int sys_getsockname(int fd, struct sockaddr *addr, size_t *addrlenptr)
{
	if (!isPointerValid((uint64_t)addrlenptr, 8, PROT_READ | PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isPointerValid((uint64_t)addr, *addrlenptr, PROT_READ | PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			out = SocketGetsockname(fp, addr, addrlenptr);
			vfsClose(fp);
		};
	};

	return out;
};

int sys_shutdown(int fd, int how)
{
	if ((how & ~SHUT_RDWR) != 0)
	{
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};

	int out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			out = ShutdownSocket(fp, how);
			vfsClose(fp);
		};
	};

	return out;
};

int sys_connect(int fd, struct sockaddr *addr, size_t addrlen)
{
	if (!isPointerValid((uint64_t)addr, addrlen, PROT_READ))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			out = ConnectSocket(fp, addr, addrlen);
			vfsClose(fp);
		};
	};

	return out;
};

int sys_getpeername(int fd, struct sockaddr *addr, size_t *addrlenptr)
{
	if (!isPointerValid((uint64_t)addrlenptr, 8, PROT_READ | PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isPointerValid((uint64_t)addr, *addrlenptr, PROT_READ | PROT_WRITE))
	{
		ERRNO = EFAULT;
	};
	
	int out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			out = SocketGetpeername(fp, addr, addrlenptr);
			vfsClose(fp);
		};
	};

	return out;
};

int sys_setsockopt(int fd, int proto, int option, uint64_t value)
{
	int out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			out = SetSocketOption(fp, proto, option, value);
			vfsClose(fp);
		};
	};

	return out;
};

uint64_t sys_getsockopt(int fd, int proto, int option)
{
	uint64_t out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			out = GetSocketOption(fp, proto, option);
			vfsClose(fp);
		};
	};

	return out;
};

int sys_uname(struct utsname *buf)
{
	if (!isPointerValid((uint64_t)buf, sizeof(struct utsname), PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	strcpy(buf->sysname, UNAME_SYSNAME);
	strcpy(buf->nodename, UNAME_NODENAME);
	strcpy(buf->release, UNAME_RELEASE);
	strcpy(buf->version, UNAME_VERSION);
	strcpy(buf->machine, UNAME_MACHINE);
	return 0;
};

int fcntl_getfd(int fd)
{
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		return -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			return -1;
		}
		else
		{
			int flags = fp->oflag;
			spinlockRelease(&ftab->spinlock);
			return flags & FD_ALL;
		};
	};
};

int fcntl_setfd(int fd, int flags)
{
	if ((flags & ~FD_ALL) != 0)
	{
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};
	
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		return -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			return -1;
		}
		else
		{
			fp->oflag = (fp->oflag & ~FD_ALL) | flags;
			spinlockRelease(&ftab->spinlock);
			return 0;
		};
	};
};

int sys_isatty(int fd)
{
	// TODO: detect of 'fd' is a terminal; for now, nothing is reported as a terminal
	ERRNO = EINVAL;
	return -1;
};

int sys_bindif(int fd, const char *ifname)
{
	if (!isStringValid((uint64_t)ifname))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		out = -1;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);
		File *fp = ftab->entries[fd];
		if (fp == NULL)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EBADF;
			out = -1;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			out = SocketBindif(fp, ifname);
			vfsClose(fp);
		};
	};

	return out;
};

int sys_thsync(int type, int par)
{
	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	int i;
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		if (ftab->entries[i] == NULL)
		{
			break;
		};
	};

	if (i == MAX_OPEN_FILES)
	{
		spinlockRelease(&ftab->spinlock);
		return sysOpenErrno(VFS_FILE_LIMIT_EXCEEDED);
	};
	
	File *fp = thsync(type, par);
	if (fp == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		// errno set by thsync()
		return -1;
	};
	
	ftab->entries[i] = fp;
	spinlockRelease(&ftab->spinlock);
	return i;
};

int sys_condwait(uint8_t *cond, uint8_t *lock)
{
	if (!isPointerValid((uint64_t)cond, 1, PROT_READ | PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	if (!isPointerValid((uint64_t)lock, 1, PROT_READ | PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};

	*cond = *cond;
	*lock = *lock;
	
	// this system call allows userspace to implement blocking locks.
	// it atomically does the following:
	// 1) store "1" in *cond, while reading out its old value.
	// 2) if the old value was 0, store 0 in *lock and return 0.
	// 3) otherwise, store 0 in *lock, and block, waiting for a signal.
	// 4) once unblocked, return -1 and set errno to EINTR.
	cli();
	lockSched();
	
	if (atomic_swap8(cond, 1) == 0)
	{
		*lock = 0;
		unlockSched();
		sti();
		
		return 0;
	};
	
	*lock = 0;
	//getCurrentThread()->flags |= THREAD_WAITING;
	waitThread(getCurrentThread());
	unlockSched();
	kyield();
	
	ERRNO = EINTR;
	return -1;
};

void signalOnBadPointer(Regs *regs, uint64_t ptr)
{
	siginfo_t siginfo;
	siginfo.si_signo = SIGSEGV;
	siginfo.si_code = SEGV_ACCERR;
	siginfo.si_addr = (void*) ptr;
	(void)siginfo;

	ERRNO = EINTR;
	*((int64_t*)&regs->rax) = -1;
	
	cli();
	lockSched();
	sendSignal(getCurrentThread(), &siginfo);
	unlockSched();
	kyield();
};

void sysInvalid()
{
	siginfo_t si;
	si.si_signo = SIGSYS;

	cli();
	lockSched();
	sendSignal(getCurrentThread(), &si);
	unlockSched();
	
	Thread *me = getCurrentThread();
	Regs regs;
	initUserRegs(&regs);
	regs.rax = 0;
	regs.rbx = me->urbx;
	regs.rbp = me->urbp;
	regs.rsp = me->ursp;
	regs.r12 = me->ur12;
	regs.r13 = me->ur13;
	regs.r14 = me->ur14;
	regs.r15 = me->ur15;
	regs.rip = me->urip;
	regs.rflags = getFlagsRegister(); 
	cli();
	switchTask(&regs);
};

void dumpRunqueue();

static uint32_t uniqueAssigner = 0;
uint32_t sys_unique()
{
	return __sync_fetch_and_add(&uniqueAssigner, 1);
};

unsigned sys_alarm(unsigned sec)
{
	uint64_t currentTime = getNanotime();
	uint64_t timeToSignal = currentTime + (uint64_t) sec * NANO_PER_SEC;
	uint64_t old = atomic_swap64(&getCurrentThread()->alarmTime, timeToSignal);
	
	if (old == 0) return 0;
	return (unsigned) ((old - currentTime) / NANO_PER_SEC);
};

int sys_exec(const char *path, const char *pars, uint64_t parsz)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isPointerValid((uint64_t)pars, parsz, PROT_READ))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return elfExec(path, pars, parsz);
};

int sys_getpid()
{
	return getCurrentThread()->pid;
};

uid_t sys_getuid()
{
	return getCurrentThread()->ruid;
};

uid_t sys_geteuid()
{
	return getCurrentThread()->euid;
};

uid_t sys_getsuid()
{
	return getCurrentThread()->suid;
};

gid_t sys_getgid()
{
	return getCurrentThread()->rgid;
};

gid_t sys_getegid()
{
	return getCurrentThread()->egid;
};

gid_t sys_getsgid()
{
	return getCurrentThread()->sgid;
};

uint64_t sys_sighandler(uint64_t handler)
{
	getCurrentThread()->rootSigHandler = handler;
	return 0;
};

void sys_sigret(void *ret)
{
	if (!isPointerValid((uint64_t)ret, sizeSignalStackFrame, PROT_READ))
	{
		ERRNO = EFAULT;
	}
	else
	{
		sigret(ret);
	};
};

uint64_t sys_getparsz()
{
	return getCurrentThread()->szExecPars;
};

void sys_getpars(void *buffer, size_t size)
{
	if (!isPointerValid((uint64_t)buffer, size, PROT_WRITE))
	{
		return;
	};
	if (size > getCurrentThread()->szExecPars)
	{
		size = getCurrentThread()->szExecPars;
	};
	memcpy(buffer, getCurrentThread()->execPars, size);
};

int sys_geterrno()
{
	return ERRNO;
};

void sys_seterrno(int num)
{
	ERRNO = num;
};

int sys_clone(int flags, MachineState *state)
{
	if (state != NULL)
	{
		if (!isPointerValid((uint64_t)state, sizeof(MachineState), PROT_READ))
		{
			ERRNO = EFAULT;
			return -1;
		};
	};
	
	Thread *me = getCurrentThread();
	Regs regs;
	initUserRegs(&regs);
	regs.rbx = me->urbx;
	regs.rbp = me->urbp;
	regs.rsp = me->ursp;
	regs.r12 = me->ur12;
	regs.r13 = me->ur13;
	regs.r14 = me->ur14;
	regs.r15 = me->ur15;
	regs.rip = me->urip;
	regs.rflags = getFlagsRegister();
	return threadClone(&regs, flags, state);
};

int sys_waitpid(int pid, int *stat_loc, int flags)
{
	if (stat_loc != NULL)
	{
		if (!isPointerValid((uint64_t)stat_loc, sizeof(int), PROT_WRITE))
		{
			ERRNO = EFAULT;
			return -1;
		};
	};
	
	// DO NOT pass the status location directly to pollThread(), because it locks scheduling
	// and writing to stat_loc might cause a page fault (which results in load-on-demand and
	// a possible kyield() etc)
	int statret;
	int ret = pollThread(pid, &statret, flags);
	if (stat_loc != NULL) *stat_loc = statret;
	return ret;
};

void sys_yield()
{
	kyield();
};

char* sys_realpath(const char *path, char *buffer)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return NULL;
	};
	
	if (!isPointerValid((uint64_t)buffer, 256, PROT_WRITE))
	{
		ERRNO = EFAULT;
		return NULL;
	};
	
	ERRNO = ENAMETOOLONG;
	return realpath(path, buffer);
};

void sys_seterrnoptr(int *ptr)
{
	if (!isPointerValid((uint64_t)ptr, sizeof(int), PROT_READ | PROT_WRITE))
	{
		return;
	};
	getCurrentThread()->errnoptr = ptr;
};

int* sys_geterrnoptr()
{
	return getCurrentThread()->errnoptr;
};

int sys_libopen(const char *path, uint64_t loadAddr, libInfo *info)
{
	if (!isPointerValid((uint64_t)info, sizeof(libInfo), PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	return libOpen(path, loadAddr, info);
};

void sys_libclose(libInfo *info)
{
	if (!isPointerValid((uint64_t)info, sizeof(libInfo), PROT_READ | PROT_WRITE))
	{
		ERRNO = EFAULT;
		return;
	};
	libClose(info);
};

int sys_unmount(const char *path)
{
	if (!isStringValid((uint64_t)path))
	{
		ERRNO = EFAULT;
		return -1;
	};

	ERRNO = EIO;	
	return unmount(path);
};

ssize_t sys_send(int socket, const void *buffer, size_t length, int flags)
{
	return sys_sendto(socket, buffer, length, flags, NULL, 0);
};

ssize_t sys_recv(int socket, void *buffer, size_t length, int flags)
{
	return sys_recvfrom(socket, buffer, length, flags, NULL, NULL);
};

int sys_route_add(int a, int b, gen_route *c)
{
	if (!isPointerValid((uint64_t)c, sizeof(gen_route), PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	return route_add(a, b, c);
};

ssize_t sys_netconf_stat(const char *a, NetStat *b, size_t c)
{
	if (!isPointerValid((uint64_t)b, c, PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	return netconf_stat(a, b, c);
};

ssize_t sys_netconf_getaddrs(const char *a, int b, void *c, size_t d)
{
	if (!isPointerValid((uint64_t)c, d, PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	return netconf_getaddrs(a, b, c, d);
};

ssize_t sys_netconf_statidx(unsigned int a, NetStat *b, size_t c)
{
	if (!isPointerValid((uint64_t)b, c, PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	return netconf_statidx(a, b, c);
};

int sys_getgroups(int count, gid_t *buffer)
{
	if (!isPointerValid((uint64_t)buffer, sizeof(gid_t)*count, PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	if (count <= getCurrentThread()->numGroups)
	{
		memcpy(buffer, getCurrentThread()->groups, sizeof(gid_t)*count);
	}
	else
	{
		memcpy(buffer, getCurrentThread()->groups, sizeof(gid_t)*getCurrentThread()->numGroups);
	};
	return getCurrentThread()->numGroups;
};

int sys_setgroups(int count, const gid_t *groups)
{
	if (!isPointerValid((uint64_t)groups, sizeof(gid_t)*count, PROT_READ))
	{
		ERRNO = EFAULT;
		return -1;
	};
	if (getCurrentThread()->egid != 0)
	{
		ERRNO = EPERM;
		return -1;
	};
	if (count > 16)
	{
		ERRNO = EINVAL;
		return -1;
	};
	memcpy(getCurrentThread()->groups, groups, sizeof(gid_t)*count);
	getCurrentThread()->numGroups = count;
	return 0;
};

int sys_netconf_addr(int a, const char *b, void *c, uint64_t d)
{
	if (!isPointerValid((uint64_t)c, d, PROT_READ))
	{
		ERRNO = EFAULT;
		return -1;
	};
	if (!isStringValid((uint64_t)b))
	{
		ERRNO = EFAULT;
		return -1;
	};
	return netconf_addr(a, b, c, d);
};

int sys_route_clear(int a, const char *b)
{
	if (!isStringValid((uint64_t)b))
	{
		ERRNO = EFAULT;
		return -1;
	};
	return route_clear(a, b);
};

int sys_munmap(uint64_t addr, uint64_t size)
{
	return mprotect(addr, size, 0);
};

int sys_getppid()
{
	return getCurrentThread()->pidParent;
};

/**
 * System call table for fast syscalls, and the number of system calls.
 */
#define SYSCALL_NUMBER 110
void* sysTable[SYSCALL_NUMBER] = {
	&sys_exit,				// 0
	&sys_write,				// 1
	&sys_exec,				// 2
	&sys_read,				// 3
	&sys_open,				// 4
	&sys_close,				// 5
	&sys_getpid,				// 6
	&sys_getuid,				// 7
	&sys_geteuid,				// 8
	&sys_getsuid,				// 9
	&sys_getgid,				// 10
	&sys_getegid,				// 11
	&sys_getsgid,				// 12
	&sys_sighandler,			// 13
	&sys_sigret,				// 14
	&sys_stat,				// 15
	&sys_getparsz,				// 16
	&sys_getpars,				// 17
	&sys_raise,				// 18
	&sys_geterrno,				// 19
	&sys_seterrno,				// 20
	&mprotect,				// 21
	&sys_lseek,				// 22
	&sys_clone,				// 23
	&sys_pause,				// 24
	&sys_waitpid,				// 25
	&signalPid,				// 26
	&sys_insmod,				// 27
	&sys_ioctl,				// 28
	&sys_fdopendir,				// 29
	NULL,					// 30 (_glidix_diag())
	&sys_mount,				// 31
	&sys_yield,				// 32
	&time,					// 33
	&sys_realpath,				// 34
	&sys_chdir,				// 35
	&sys_getcwd,				// 36
	&sys_fstat,				// 37
	&sys_chmod,				// 38
	&sys_fchmod,				// 39
	&sys_fsync,				// 40
	&sys_chown,				// 41
	&sys_fchown,				// 42
	&sys_mkdir,				// 43
	&sys_ftruncate,				// 44
	&sys_unlink,				// 45
	&sys_dup,				// 46
	&sys_dup2,				// 47
	&sys_pipe,				// 48
	&sys_seterrnoptr,			// 49
	&sys_geterrnoptr,			// 50
	&getNanotime,				// 51
	&sys_libopen,				// 52
	&sys_libclose,				// 53
	&sys_mmap,				// 54
	&setuid,				// 55
	&setgid,				// 56
	&seteuid,				// 57
	&setegid,				// 58
	&setreuid,				// 59
	&setregid,				// 60
	&sys_rmmod,				// 61
	&sys_link,				// 62
	&sys_unmount,				// 63
	&sys_lstat,				// 64
	&sys_symlink,				// 65
	&sys_readlink,				// 66
	&systemDown,				// 67
	&sys_sleep,				// 68
	&sys_utime,				// 69
	&sys_umask,				// 70
	&sysRouteTable,				// 71
	&sys_socket,				// 72
	&sys_bind,				// 73
	&sys_sendto,				// 74
	&sys_send,				// 75
	&sys_recvfrom,				// 76
	&sys_recv,				// 77
	&sys_route_add,				// 78
	&sys_netconf_stat,			// 79
	&sys_netconf_getaddrs,			// 80
	&sys_getsockname,			// 81
	&sys_shutdown,				// 82
	&sys_connect,				// 83
	&sys_getpeername,			// 84
	&sys_setsockopt,			// 85
	&sys_getsockopt,			// 86
	&sys_netconf_statidx,			// 87
	&sys_pcistat,				// 88
	&sys_getgroups,				// 89
	&sys_setgroups,				// 90
	&sys_uname,				// 91
	&sys_netconf_addr,			// 92
	&fcntl_getfd,				// 93
	&fcntl_setfd,				// 94
	&sys_unique,				// 95
	&sys_isatty,				// 96
	&sys_bindif,				// 97
	&sys_route_clear,			// 98
	&sys_munmap,				// 99
	&sys_thsync,				// 100
	&sys_getppid,				// 101
	&sys_alarm,				// 102
	&sys_condwait,				// 103
	&sys_mqserver,				// 104
	&sys_mqclient,				// 105
	&sys_mqsend,				// 106
	&sys_mqrecv,				// 107
	&sys_shmalloc,				// 108
	&sys_shmap,				// 109
};
uint64_t sysNumber = SYSCALL_NUMBER;

uint64_t sysEpilog(uint64_t retval)
{
	if (getCurrentThread()->errnoptr != NULL)
	{
		*(getCurrentThread()->errnoptr) = getCurrentThread()->therrno;
	};

	if (getCurrentThread()->sigcnt > 0)
	{
		Thread *me = getCurrentThread();
		Regs regs;
		initUserRegs(&regs);
		regs.rax = retval;
		regs.rbx = me->urbx;
		regs.rbp = me->urbp;
		regs.rsp = me->ursp;
		regs.r12 = me->ur12;
		regs.r13 = me->ur13;
		regs.r14 = me->ur14;
		regs.r15 = me->ur15;
		regs.rip = me->urip;
		regs.rflags = getFlagsRegister(); 
		cli();
		switchTask(&regs);
	};

	return retval;
};
