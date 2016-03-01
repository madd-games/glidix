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
	Regs regs;				// it's OK to destroy the registers as we're exiting
	threadExit(getCurrentThread(), status);
	ASM("cli");
	switchTask(&regs);
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
					out = fp->write(fp, buf, size);
					spinlockRelease(&ftab->spinlock);
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
					spinlockRelease(&ftab->spinlock);
					out = fp->read(fp, buf, size);
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
		vfsClose(fp);
	};

	spinlockRelease(&ftab->spinlock);
	return 0;
};

int sys_ioctl(int fd, uint64_t cmd, void *argp)
{
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

	spinlockRelease(&ftab->spinlock);
	int ret = fp->ioctl(fp, cmd, argp);
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

	off_t ret = fp->seek(fp, offset, whence);
	if (ret == (off_t)-1)
	{
		getCurrentThread()->therrno = EOVERFLOW;
	};

	spinlockRelease(&ftab->spinlock);
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

	sendSignal(getCurrentThread(), &siginfo);

	// not actually stored in RAX because of the task switch, but libglidix should set
	// RAX to zero before executing UD2.
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

void sys_pause(Regs *regs)
{
	ASM("cli");
	lockSched();
	*((int*)&regs->rax) = -1;
	getCurrentThread()->therrno = EINTR;
	getCurrentThread()->flags |= THREAD_WAITING;
	unlockSched();
	switchTask(regs);
};

int sys_fstat(int fd, struct stat *buf)
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

	if (fp->fstat == NULL)
	{
		getCurrentThread()->therrno = EIO;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	int status = fp->fstat(fp, buf);

	spinlockRelease(&ftab->spinlock);
	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
	};

	return status;
};

int sys_chmod(const char *path, mode_t mode)
{
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
	int status = fp->fstat(fp, &st);

	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	if ((getCurrentThread()->euid != 0) && (getCurrentThread()->euid != st.st_uid))
	{
		getCurrentThread()->therrno = EPERM;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	status = fp->fchmod(fp, mode);
	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	spinlockRelease(&ftab->spinlock);
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

	if (fp->fsync != NULL)
	{
		fp->fsync(fp);
	};

	spinlockRelease(&ftab->spinlock);
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
	int status = fp->fstat(fp, &st);

	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	if (!canChangeOwner(&st, uid, gid))
	{
		getCurrentThread()->therrno = EPERM;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	status = fp->fchown(fp, uid, gid);
	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
		spinlockRelease(&ftab->spinlock);
		return -1;
	};

	spinlockRelease(&ftab->spinlock);
	return 0;
};

int sys_mkdir(const char *path, mode_t mode)
{
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

	fp->truncate(fp, length);

	spinlockRelease(&ftab->spinlock);
	return 0;
};

int sys_unlink(const char *path)
{
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

	if (fp->dup == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = EIO;
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

	File *newfp = (File*) kmalloc(sizeof(File));
	memset(newfp, 0, sizeof(File));

	if (fp->dup(fp, newfp, sizeof(File)) != 0)
	{
		kfree(newfp);
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	ftab->entries[i] = newfp;
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

	if (fp->dup == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	File *newfp = (File*) kmalloc(sizeof(File));
	memset(newfp, 0, sizeof(File));
	if (fp->dup(fp, newfp, sizeof(File)) != 0)
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = EIO;
		return -1;
	};

	if (ftab->entries[newfd] != NULL)
	{
		File *toclose = ftab->entries[newfd];
		if (toclose->close != NULL) toclose->close(toclose);
		kfree(toclose);
	};
	ftab->entries[newfd] = newfp;
	spinlockRelease(&ftab->spinlock);

	return newfd;
};

int sys_insmod(const char *modname, const char *path, const char *opt, int flags)
{
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

	FrameList *fl = fp->mmap(fp, len, prot, flags, offset);
	if (fl == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = ENODEV;
		return 0;
	};

	spinlockRelease(&ftab->spinlock);

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
	getCurrentThread()->flags |= THREAD_WAITING;
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
	
	ftab->entries[i] = fp;
	spinlockRelease(&ftab->spinlock);
	return i;
};

int sys_bind(int fd, const struct sockaddr *addr, size_t addrlen)
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
			out = BindSocket(fp, addr, addrlen);
			spinlockRelease(&ftab->spinlock);
		};
	};

	return out;
};

ssize_t sys_sendto(int fd, const void *message, size_t len, int flags, const struct sockaddr *addr, size_t addrlen)
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
			out = SendtoSocket(fp, message, len, flags, addr, addrlen);
			spinlockRelease(&ftab->spinlock);
		};
	};

	return out;
};

ssize_t sys_recvfrom(int fd, void *message, size_t len, int flags, struct sockaddr *addr, size_t *addrlen)
{
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
			out = RecvfromSocket(fp, message, len, flags, addr, addrlen);
			spinlockRelease(&ftab->spinlock);
		};
	};

	return out;
};

int sys_getsockname(int fd, struct sockaddr *addr, size_t *addrlenptr)
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
			out = SocketGetsockname(fp, addr, addrlenptr);
			spinlockRelease(&ftab->spinlock);
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
			out = ShutdownSocket(fp, how);
			spinlockRelease(&ftab->spinlock);
		};
	};

	return out;
};

int sys_connect(int fd, struct sockaddr *addr, size_t addrlen)
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
			out = ConnectSocket(fp, addr, addrlen);
			spinlockRelease(&ftab->spinlock);
		};
	};

	return out;
};

int sys_getpeername(int fd, struct sockaddr *addr, size_t *addrlenptr)
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
			out = SocketGetpeername(fp, addr, addrlenptr);
			spinlockRelease(&ftab->spinlock);
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
			out = SetSocketOption(fp, proto, option, value);
			spinlockRelease(&ftab->spinlock);
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
			out = GetSocketOption(fp, proto, option);
			spinlockRelease(&ftab->spinlock);
		};
	};

	return out;
};

int sys_uname(struct utsname *buf)
{
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
			out = SocketBindif(fp, ifname);
			spinlockRelease(&ftab->spinlock);
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
	// this system call allows userspace to implement blocking locks.
	// it atomically does the following:
	// 1) store "1" in *cond, while reading out its old value.
	// 2) if the old value was 0, store 0 in *lock and return 0.
	// 3) otherwise, store 0 in *lock, and block, waiting for a signal.
	// 4) once unblocked, return -1 and set errno to EINTR.
	lockSched();
	cli();
	
	if (atomic_swap8(cond, 1) == 0)
	{
		*lock = 0;
		unlockSched();
		sti();
		
		return 0;
	};
	
	*lock = 0;
	getCurrentThread()->flags |= THREAD_WAITING;
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
	
	lockSched();
	sendSignal(getCurrentThread(), &siginfo);
	unlockSched();
	kyield();
};

void signalOnBadSyscall(Regs *regs)
{
	siginfo_t si;
	si.si_signo = SIGSYS;

	lockSched();
	sendSignal(getCurrentThread(), &si);
	unlockSched();
	kyield();
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

/**
 * System call table for fast syscalls, and the number of system calls.
 */
#define SYSCALL_NUMBER 19
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
};
uint64_t sysNumber = SYSCALL_NUMBER;

uint64_t sysEpilog(uint64_t retval)
{
	if (getCurrentThread()->errnoptr != NULL)
	{
		*(getCurrentThread()->errnoptr) = getCurrentThread()->therrno;
	};

	// TODO: signal dispatching
#if 0
	if ((getCurrentThread()->sigcnt > 0) && ((regs->cs & 3) == 3) && ((getCurrentThread()->flags & THREAD_SIGNALLED) == 0))
	{
		// also dispatching after calls.
		ASM("cli");
		lockSched();
		memcpy(&getCurrentThread()->regs, regs, sizeof(Regs));
		dispatchSignal(getCurrentThread());
		memcpy(regs, &getCurrentThread()->regs, sizeof(Regs));
		unlockSched();
		return;					// interrupts will be enabled on return
	};
#endif

	return retval;
};

void syscallDispatch(Regs *regs, uint16_t num)
{
	if (getCurrentThread()->errnoptr != NULL)
	{
		getCurrentThread()->therrno = *(getCurrentThread()->errnoptr);
	};

	if ((getCurrentThread()->sigcnt > 0) && ((regs->cs & 3) == 3) && ((getCurrentThread()->flags & THREAD_SIGNALLED) == 0))
	{
		// before doing any syscalls, we dispatch all signals.
		ASM("cli");
		lockSched();
		memcpy(&getCurrentThread()->regs, regs, sizeof(Regs));
		dispatchSignal(getCurrentThread());
		memcpy(regs, &getCurrentThread()->regs, sizeof(Regs));
		unlockSched();
		return;					// interrupts will be enabled on return
	};
	
	regs->rip += 4;

	// store the return registers; this is in case some syscall, such as read(), requests a return-on-signal.
	memcpy(&getCurrentThread()->intSyscallRegs, regs, sizeof(Regs));

	// for the magical pipe() system call
	int pipefd[2];
	
	switch (num)
	{
	case 0:
		sys_exit((int) regs->rdi);
		break;					/* never actually reached */
	case 1:
		*((ssize_t*)&regs->rax) = sys_write(*((int*)&regs->rdi), (const void*) regs->rsi, *((size_t*)&regs->rdx));
		break;
	case 2:
		*((int*)&regs->rax) = sys_exec((const char*)regs->rdi, (const char*) regs->rsi, regs->rdx);
		break;
	case 3:
		*((ssize_t*)&regs->rax) = sys_read(*((int*)&regs->rdi), (void*) regs->rsi, *((size_t*)&regs->rdx));
		break;
	case 4:
		*((int*)&regs->rax) = sys_open((const char*) regs->rdi, (int) regs->rsi, (mode_t) regs->rdx);
		kprintf_debug("[*] sys_open: '%s'\n", (const char*) regs->rdi);
		break;
	case 5:
		regs->rax = 0;
		sys_close((int)regs->rdi);
		break;
	case 6:
		regs->rax = getCurrentThread()->pid;
		break;
	case 7:
		regs->rax = getCurrentThread()->ruid;
		break;
	case 8:
		regs->rax = getCurrentThread()->euid;
		break;
	case 9:
		regs->rax = getCurrentThread()->suid;
		break;
	case 10:
		regs->rax = getCurrentThread()->rgid;
		break;
	case 11:
		regs->rax = getCurrentThread()->egid;
		break;
	case 12:
		regs->rax = getCurrentThread()->sgid;
		break;
	case 13:
		getCurrentThread()->rootSigHandler = regs->rdi;
		regs->rax = 0;
		break;
	case 14:
		if (!isPointerValid(regs->rdi, sizeSignalStackFrame, PROT_READ))
		{
			signalOnBadPointer(regs, regs->rdi);
			break;
		};
		sigret((void*) regs->rdi);
		break;
	case 15:
		*((int*)&regs->rax) = sys_stat((const char*) regs->rdi, (struct stat*) regs->rsi);
		//kprintf_debug("[*] sys_stat: '%s', returned %d\n", (const char*) regs->rdi, *((int*)&regs->rax));
		break;
	case 16:
		regs->rax = (uint64_t) getCurrentThread()->szExecPars;
		break;
	case 17:
		if (!isPointerValid(regs->rdi, regs->rsi, PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdi);
			break;
		};
		memcpy((void*)regs->rdi, getCurrentThread()->execPars, regs->rsi);
		break;
	case 18:
		*((int*)&regs->rax) = sys_raise((int) regs->rdi);
		break;
	case 19:
		*((int*)&regs->rax) = getCurrentThread()->therrno;
		break;
	case 20:
		getCurrentThread()->therrno = *((int*)&regs->rdi);
		break;
	case 21:
		*((int*)&regs->rax) = mprotect(regs->rdi, regs->rsi, (int) regs->rdx);
		break;
	case 22:
		*((off_t*)&regs->rax) = sys_lseek((int) regs->rdi, *((off_t*)&regs->rsi), (int) regs->rdx);
		break;
	case 23:
		if (kernelStatus == KERNEL_STOPPING)
		{
			getCurrentThread()->therrno = EPERM;
			*((int*)&regs->rax) = -1;
			break;
		};
		*((int*)&regs->rax) = threadClone(regs, (int) regs->rdi, (MachineState*) regs->rsi);
		break;
	case 24:
		sys_pause(regs);
		break;
	case 25:
		// yes, the second argument is a pointer to RDI :)
		*((int*)&regs->rax) = pollThread(regs, *((int*)&regs->rdi), (int*) &regs->rdi, (int) regs->rdx);
		break;
	case 26:
		*((int*)&regs->rax) = signalPid((int) regs->rdi, (int) regs->rsi);
		break;
	case 27:
		*((int*)&regs->rax) = sys_insmod((const char*) regs->rdi, (const char*) regs->rsi, (const char*) regs->rdx, (int) regs->rcx);
		break;
	case 28:
		if (!isPointerValid(regs->rdx, (regs->rsi >> 32) & 0xFFFF, PROT_READ | PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdx);
			break;
		};
		*((int*)&regs->rax) = sys_ioctl((int) regs->rdi, regs->rsi, (void*) regs->rdx);
		break;
	case 29:
		*((int*)&regs->rax) = sys_fdopendir((const char*) regs->rdi);
		break;
	case 30:
		/* _glidix_diag */
		//heapDump();
		//kdumpregs(regs);
		//dumpRunqueue();
		//regs->rflags &= ~(1 << 9);
		//BREAKPOINT();
		//dumpModules();
		break;
	case 31:
		*((int*)&regs->rax) = sys_mount((const char*)regs->rdi, (const char*)regs->rsi, (const char*)regs->rdx, (int)regs->rcx);
		break;
	case 32:
		/* _glidix_yield */
		//ASM("cli");
		//switchTask(regs);
		break;
	case 33:
		*((time_t*)&regs->rax) = time();
		break;
	case 34:
		if (!isPointerValid(regs->rsi, 256, PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		regs->rax = (uint64_t) realpath((const char*) regs->rdi, (char*) regs->rsi);
		if (regs->rax == 0)
		{
			getCurrentThread()->therrno = ENAMETOOLONG;
		};
		break;
	case 35:
		*((int*)&regs->rax) = sys_chdir((const char*)regs->rdi);
		break;
	case 36:
		if (!isPointerValid(regs->rdi, regs->rsi, PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdi);
			break;
		};
		regs->rax = (uint64_t) sys_getcwd((char*) regs->rdi, regs->rsi);
		break;
	case 37:
		if (!isPointerValid(regs->rsi, sizeof(struct stat), PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((int*)&regs->rax) = sys_fstat((int)regs->rdi, (struct stat*) regs->rsi);
		break;
	case 38:
		*((int*)&regs->rax) = sys_chmod((const char*) regs->rdi, (mode_t) regs->rsi);
		break;
	case 39:
		*((int*)&regs->rax) = sys_fchmod((int) regs->rdi, (mode_t) regs->rsi);
		break;
	case 40:
		*((int*)&regs->rax) = sys_fsync((int)regs->rdi);
		break;
	case 41:
		*((int*)&regs->rax) = sys_chown((const char*)regs->rdi, (uid_t)regs->rsi, (gid_t)regs->rdx);
		break;
	case 42:
		*((int*)&regs->rax) = sys_fchown((int)regs->rdi, (uid_t)regs->rsi, (gid_t)regs->rdx);
		break;
	case 43:
		*((int*)&regs->rax) = sys_mkdir((const char*)regs->rdi, (mode_t)regs->rsi);
		break;
	case 44:
		*((int*)&regs->rax) = sys_ftruncate((int)regs->rdi, (off_t)regs->rsi);
		break;
	case 45:
		*((int*)&regs->rax) = sys_unlink((const char*)regs->rdi);
		break;
	case 46:
		*((int*)&regs->rax) = sys_dup((int)regs->rdi);
		break;
	case 47:
		*((int*)&regs->rax) = sys_dup2((int)regs->rdi, (int)regs->rsi);
		break;
	case 48:
		*((int*)&regs->rax) = sys_pipe(pipefd);
		*((int*)&regs->r8) = pipefd[0];
		*((int*)&regs->r9) = pipefd[1];
		break;
	case 49:
		if (!isPointerValid(regs->rdi, sizeof(int), PROT_READ | PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdi);
			break;
		};
		getCurrentThread()->errnoptr = (int*) regs->rdi;
		break;
	case 50:
		regs->rax = (uint64_t) getCurrentThread()->errnoptr;
		break;
	case 51:
		*((clock_t*)&regs->rax) = (clock_t) getUptime() * 1000000;
		break;
	case 52:
		if (!isPointerValid(regs->rdx, sizeof(libInfo), PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdx);
			break;
		};
		*((int*)&regs->rax) = libOpen((const char*) regs->rdi, regs->rsi, (libInfo*) regs->rdx);
		break;
	case 53:
		if (!isPointerValid(regs->rdi, sizeof(libInfo), PROT_READ | PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdi);
			break;
		};
		libClose((libInfo*) regs->rdi);
		break;
	case 54:
		regs->rax = sys_mmap(regs->rdi, (size_t)regs->rsi, (int)regs->rdx, (int)regs->rcx, (int)regs->r8, (off_t)regs->r9);
		break;
	case 55:
		regs->rax = setuid(regs->rdi);
		break;
	case 56:
		regs->rax = setgid(regs->rdi);
		break;
	case 57:
		regs->rax = seteuid(regs->rdi);
		break;
	case 58:
		regs->rax = setegid(regs->rdi);
		break;
	case 59:
		regs->rax = setreuid(regs->rdi, regs->rsi);
		break;
	case 60:
		regs->rax = setregid(regs->rdi, regs->rsi);
		break;
	case 61:
		*((int*)&regs->rax) = sys_rmmod((const char*) regs->rdi, (int) regs->rsi);
		break;
	case 62:
		*((int*)&regs->rax) = sys_link((const char*) regs->rdi, (const char*) regs->rsi);
		break;
	case 63:
		*((int*)&regs->rax) = unmount((const char*) regs->rdi);
		break;
	case 64:
		if (!isPointerValid(regs->rsi, sizeof(struct stat), PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((int*)&regs->rax) = sys_lstat((const char*) regs->rdi, (struct stat*) regs->rsi);
		break;
	case 65:
		*((int*)&regs->rax) = sys_symlink((const char*) regs->rdi, (const char*) regs->rsi);
		break;
	case 66:
		if (!isPointerValid(regs->rsi, regs->rdx, PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((ssize_t*)&regs->rax) = sys_readlink((const char*) regs->rdi, (char*) regs->rsi, (size_t) regs->rdx);
		break;
	case 67:
		*((int*)&regs->rax) = systemDown((int) regs->rdi);
		break;
	case 68:
		*((unsigned*)&regs->rax) = sys_sleep((unsigned) regs->rdi);
		break;
	case 69:
		*((int*)&regs->rax) = sys_utime((const char*) regs->rdi, (time_t) regs->rsi, (time_t) regs->rdx);
		break;
	case 70:
		*((mode_t*)&regs->rax) = sys_umask((mode_t) regs->rdi);
		break;
	case 71:
		*((int*)&regs->rax) = sysRouteTable(regs->rdi);
		break;
	case 72:
		*((int*)&regs->rax) = sys_socket((int) regs->rdi, (int) regs->rsi, (int) regs->rdx);
		break;
	case 73:
		if (!isPointerValid(regs->rsi, regs->rdx, PROT_READ))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((int*)&regs->rax) = sys_bind((int) regs->rdi, (const struct sockaddr*) regs->rsi, (size_t) regs->rdx);
		break;
	case 74:
		if (!isPointerValid(regs->rsi, regs->rdx, PROT_READ))	// message
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		if (!isPointerValid(regs->r8, regs->r9, PROT_READ))	// addr
		{
			signalOnBadPointer(regs, regs->r8);
			break;
		};
		*((ssize_t*)&regs->rax) = sys_sendto((int) regs->rdi, (const void*) regs->rsi, (size_t) regs->rdx,
							(int) regs->rcx, (const struct sockaddr*) regs->r8, (size_t) regs->r9);
		break;
	case 75:	// send
		if (!isPointerValid(regs->rsi, regs->rdx, PROT_READ))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((ssize_t*)&regs->rax) = sys_sendto((int) regs->rdi, (const void*) regs->rsi, (size_t) regs->rdx, (int) regs->rcx, NULL, 0);
		break;
	case 76:
		if (!isPointerValid(regs->rsi, regs->rdx, PROT_WRITE))	// message
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		if (!isPointerValid(regs->r9, 8, PROT_READ | PROT_WRITE))		// addrlen
		{
			signalOnBadPointer(regs, regs->r9);
			break;
		};
		if (!isPointerValid(regs->r8, *((uint64_t*)regs->r9), PROT_WRITE)) // addr
		{
			signalOnBadPointer(regs, regs->r8);
			break;
		};
		*((ssize_t*)&regs->rax) = sys_recvfrom((int) regs->rdi, (void*) regs->rsi, (size_t) regs->rdx,
							(int) regs->rcx, (struct sockaddr*) regs->r8, (size_t*) regs->r9);
		break;
	case 77:
		if (!isPointerValid(regs->rsi, regs->rdx, PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((ssize_t*)&regs->rax) = sys_recvfrom((int) regs->rdi, (void*) regs->rsi, (size_t) regs->rdx, (int) regs->rcx, NULL, NULL);
		break;
	case 78:
		if (!isPointerValid(regs->rdx, sizeof(gen_route), PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdx);
			break;
		};
		*((int*)&regs->rax) = route_add((int) regs->rdi, *((int*)&regs->rsi), (gen_route*) regs->rdx);
		break;
	case 79:
		if (!isPointerValid(regs->rsi, regs->rdx, PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((ssize_t*)&regs->rax) = netconf_stat((const char*) regs->rdi, (NetStat*) regs->rsi, (size_t) regs->rdx);
		break;
	case 80:
		if (!isPointerValid(regs->rdx, regs->rcx, PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdx);
			break;
		};
		*((ssize_t*)&regs->rax) = netconf_getaddrs((const char*) regs->rdi, (int) regs->rsi, (void*) regs->rdx, (size_t) regs->rcx);
		break;
	case 81:
		if (!isPointerValid(regs->rdx, 8, PROT_READ | PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdx);
			break;
		};
		if (!isPointerValid(regs->rsi, *((uint64_t*)regs->rdx), PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((int*)&regs->rax) = sys_getsockname((int) regs->rdi, (struct sockaddr*) regs->rsi, (size_t*) regs->rdx);
		break;
	case 82:
		*((int*)&regs->rax) = sys_shutdown((int) regs->rdi, (int) regs->rsi);
		break;
	case 83:
		if (!isPointerValid(regs->rsi, regs->rdx, PROT_READ))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((int*)&regs->rax) = sys_connect((int) regs->rdi, (struct sockaddr*) regs->rsi, (size_t) regs->rdx);
		break;
	case 84:
		if (!isPointerValid(regs->rdx, 8, PROT_READ | PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdx);
			break;
		};
		if (!isPointerValid(regs->rsi, *((uint64_t*)regs->rdx), PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((int*)&regs->rax) = sys_getpeername((int) regs->rdi, (struct sockaddr*) regs->rsi, (size_t*) regs->rdx);
		break;
	case 85:
		*((int*)&regs->rax) = sys_setsockopt((int) regs->rdi, (int) regs->rsi, (int) regs->rdx, regs->rcx);
		break;
	case 86:
		regs->rax = sys_getsockopt((int) regs->rdi, (int) regs->rsi, (int) regs->rdx);
		break;
	case 87:
		if (!isPointerValid(regs->rsi, regs->rdx, PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((ssize_t*)&regs->rax) = netconf_statidx((unsigned int) regs->rdi, (NetStat*) regs->rsi, (size_t) regs->rdx);
		break;
	case 88:
		if (!isPointerValid(regs->rsi, regs->rdx, PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((ssize_t*)&regs->rax) = sys_pcistat((int) regs->rdi, (PCIDevice*) regs->rsi, (size_t) regs->rdx);
		break;
	case 89:
		/* getgroups */
		if (!isPointerValid(regs->rsi, sizeof(gid_t)*(*((int*)&regs->rdi)), PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		if (*((int*)&regs->rdi) <= getCurrentThread()->numGroups)
		{
			memcpy((void*)regs->rsi, getCurrentThread()->groups, sizeof(gid_t)*(*((int*)&regs->rdi)));
		}
		else
		{
			memcpy((void*)regs->rsi, getCurrentThread()->groups, sizeof(gid_t)*getCurrentThread()->numGroups);
		};
		*((int*)&regs->rax) = getCurrentThread()->numGroups;
		break;
	case 90:
		/* _glidix_setgroups */
		if (!isPointerValid(regs->rsi, sizeof(gid_t)*regs->rdi, PROT_READ))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		if (getCurrentThread()->egid != 0)
		{
			ERRNO = EPERM;
			*((int*)&regs->rax) = -1;
			break;
		};
		if (regs->rdi > 16)
		{
			ERRNO = EINVAL;
			*((int*)regs->rax) = -1;
			break;
		};
		memcpy(getCurrentThread()->groups, (const void*)regs->rsi, sizeof(gid_t)*regs->rdi);
		getCurrentThread()->numGroups = regs->rdi;
		*((int*)&regs->rax) = 0;
		break;
	case 91:
		if (!isPointerValid(regs->rdi, sizeof(struct utsname), PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdi);
			break;
		};
		*((int*)&regs->rax) = sys_uname((struct utsname*)regs->rdi);
		break;
	case 92:
		if (!isPointerValid(regs->rdx, regs->rcx, PROT_READ))
		{
			*((int*)&regs->rax) = -1;
			ERRNO = EFAULT;
			break;
		};
		if (!isStringValid(regs->rsi))
		{
			*((int*)&regs->rax) = -1;
			ERRNO = EFAULT;
			break;
		};
		*((int*)&regs->rax) = netconf_addr((int)regs->rdi, (const char*)regs->rsi, (void*)regs->rdx, regs->rcx);
		break;
	case 93:
		*((int*)&regs->rax) = fcntl_getfd((int)regs->rdi);
		break;
	case 94:
		*((int*)&regs->rax) = fcntl_setfd((int)regs->rdi, (int)regs->rsi);
		break;
	case 95:
		regs->rax = (uint64_t) sys_unique();
		break;
	case 96:
		*((int*)&regs->rax) = sys_isatty((int) regs->rdi);
		break;
	case 97:
		if (!isStringValid(regs->rsi))
		{
			*((int*)&regs->rax) = -1;
			ERRNO = EFAULT;
			break;
		};
		*((int*)&regs->rax) = sys_bindif((int)regs->rdi, (const char*)regs->rsi);
		break;
	case 98:
		if (!isStringValid(regs->rsi))
		{
			*((int*)&regs->rax) = -1;
			ERRNO = EFAULT;
			break;
		};
		*((int*)&regs->rax) = route_clear((int)regs->rdi, (const char*) regs->rsi);
		break;
	case 99:
		*((int*)&regs->rax) = mprotect(regs->rdi, regs->rsi, 0);
		break;
	case 100:
		*((int*)&regs->rax) = sys_thsync((int)regs->rdi, (int)regs->rsi);
		break;
	case 101:
		regs->rax = getCurrentThread()->pidParent;
		break;
	case 102:
		*((unsigned*)&regs->rax) = sys_alarm(*((unsigned*)&regs->rdi));
		break;
	case 103:
		if (!isPointerValid(regs->rdi, 1, PROT_READ | PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rdi);
			break;
		};
		if (!isPointerValid(regs->rsi, 1, PROT_READ | PROT_WRITE))
		{
			signalOnBadPointer(regs, regs->rsi);
			break;
		};
		*((int*)&regs->rax) = sys_condwait((uint8_t*)regs->rdi, (uint8_t*)regs->rsi);
		break;
	case 104:
		*((int*)&regs->rax) = sys_mqserver();
		break;
	case 105:
		*((int*)&regs->rax) = sys_mqclient((int)regs->rdi, (int)regs->rsi);
		break;
	case 106:
		if (!isPointerValid(regs->rcx, regs->r8, PROT_READ))
		{
			ERRNO = EFAULT;
			*((int*)&regs->rax) = -1;
		};
		*((int*)&regs->rax) = sys_mqsend((int)regs->rdi, (int)regs->rsi, (int)regs->rdx, (const void*)regs->rcx, (size_t)regs->r8);
		break;
	case 107:
		if (!isPointerValid(regs->rsi, sizeof(MessageInfo), PROT_WRITE))
		{
			ERRNO = EFAULT;
			*((int*)&regs->rax) = -1;
		};
		if (!isPointerValid(regs->rdx, regs->rcx, PROT_WRITE))
		{
			ERRNO = EFAULT;
			*((int*)&regs->rax) = -1;
		};
		*((ssize_t*)&regs->rax) = sys_mqrecv((int)regs->rdi, (MessageInfo*)regs->rsi, (void*)regs->rdx, (size_t)regs->rcx);
		break;
	case 108:
		regs->rax = sys_shmalloc(regs->rdi, regs->rsi, *((int*)&regs->rdx), *((int*)&regs->rcx), *((int*)&regs->r8));
		break;
	case 109:
		*((int*)&regs->rax) = sys_shmap(regs->rdi, regs->rsi, regs->rdx, *((int*)&regs->rcx));
		break;
	default:
		signalOnBadSyscall(regs);
		break;
	};

	if (getCurrentThread()->errnoptr != NULL)
	{
		*(getCurrentThread()->errnoptr) = getCurrentThread()->therrno;
	};

	if ((getCurrentThread()->sigcnt > 0) && ((regs->cs & 3) == 3) && ((getCurrentThread()->flags & THREAD_SIGNALLED) == 0))
	{
		// also dispatching after calls.
		ASM("cli");
		lockSched();
		memcpy(&getCurrentThread()->regs, regs, sizeof(Regs));
		dispatchSignal(getCurrentThread());
		memcpy(regs, &getCurrentThread()->regs, sizeof(Regs));
		unlockSched();
		return;					// interrupts will be enabled on return
	};
};
