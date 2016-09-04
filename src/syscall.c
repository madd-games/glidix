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
#include <glidix/down.h>
#include <glidix/netif.h>
#include <glidix/socket.h>
#include <glidix/pci.h>
#include <glidix/utsname.h>
#include <glidix/thsync.h>
#include <glidix/message.h>
#include <glidix/shmem.h>
#include <glidix/catch.h>

/**
 * Options for _glidix_kopt().
 */
#define	_GLIDIX_KOPT_GFXTERM		0		/* whether the graphics terminal is enabled */

int memcpy_u2k(void *dst_, const void *src_, size_t size)
{
	// user to kernel: src is in userspace
	uint64_t start = (uint64_t) src_;
	uint64_t end = (uint64_t) src_ + size;
	
	if ((start < 0x1000) || (start >= 0x7FC0000000))
	{
		return -1;
	};
	
	if ((end < 0x1000) || (end >= 0x7FC0000000))
	{
		return -1;
	};
	
	if (end < start)
	{
		// overflow
		return -1;
	};
	
	int ex = catch();
	if (ex == 0)
	{
		memcpy(dst_, src_, size);
		uncatch();
		return 0;
	}
	else
	{
		return -1;
	};
};

int memcpy_k2u(void *dst_, const void *src_, size_t size)
{
	// kernel to user: dst is in userspace
	uint64_t start = (uint64_t) dst_;
	uint64_t end = (uint64_t) dst_ + size;
	
	if ((start < 0x1000) || (start >= 0x7FC0000000))
	{
		return -1;
	};
	
	if ((end < 0x1000) || (end >= 0x7FC0000000))
	{
		return -1;
	};
	
	if (end < start)
	{
		// overflow
		return -1;
	};
	
	int ex = catch();
	if (ex == 0)
	{
		memcpy(dst_, src_, size);
		uncatch();
		return 0;
	}
	else
	{
		return -1;
	};
};

int strcpy_u2k(char *dst, const char *src)
{
	// src is in userspace
	uint64_t addr = (uint64_t) src;
	
	size_t count = 0;
	while (1)
	{
		if (addr >= 0x7FC0000000)
		{
			return -1;
		};
		
		if (count == (USER_STRING_MAX-1))
		{
			return -1;
		};
		
		char c = *((char*)addr);
		addr++;
		*dst++ = c;
		if (c == 0) return 0;
	};
};

void sys_exit(int status)
{
	processExit(status);
	kyield();
};

ssize_t sys_write(int fd, const void *buf, size_t size)
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
					void *tmpbuf = kmalloc(size);
					if (tmpbuf == NULL)
					{
						vfsClose(fp);
						ERRNO = ENOBUFS;
						out = -1;
					}
					else if (memcpy_u2k(tmpbuf, buf, size) != 0)
					{
						vfsClose(fp);
						kfree(tmpbuf);
						ERRNO = EFAULT;
						out = -1;
					}
					else
					{
						out = fp->write(fp, tmpbuf, size);
						vfsClose(fp);
						kfree(tmpbuf);
					};
				};
			};
		};
	};

	return out;
};

uint64_t sys_pwrite(int fd, const void *buf, size_t size, off_t offset)
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
			if (fp->pwrite == NULL)
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
					void *tmpbuf = kmalloc(size);
					if (tmpbuf == NULL)
					{
						vfsClose(fp);
						ERRNO = ENOBUFS;
						return -1;
					};
					
					if (memcpy_u2k(tmpbuf, buf, size) != 0)
					{
						vfsClose(fp);
						kfree(tmpbuf);
						ERRNO = EFAULT;
						return -1;
					}
					else
					{
						out = fp->pwrite(fp, tmpbuf, size, offset);
						vfsClose(fp);
						kfree(tmpbuf);
					};
				};
			};
		};
	};

	return *((uint64_t*)&out);
};

ssize_t sys_read(int fd, void *buf, size_t size)
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
					void *tmpbuf = kmalloc(size);
					if (tmpbuf == NULL)
					{
						vfsClose(fp);
						ERRNO = ENOBUFS;
						return -1;
					};
					
					out = fp->read(fp, tmpbuf, size);
					size_t toCopy = (size_t) out;
					if (out == -1) toCopy = 0;
					vfsClose(fp);
					if (memcpy_k2u(buf, tmpbuf, toCopy) != 0)
					{
						ERRNO = EFAULT;
						out = -1;
					};
					kfree(tmpbuf);
				};
			};
		};
	};

	return out;
};

ssize_t sys_pread(int fd, void *buf, size_t size, off_t offset)
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
			if (fp->pread == NULL)
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
					void *tmpbuf = kmalloc(size);
					if (tmpbuf == NULL)
					{
						vfsClose(fp);
						ERRNO = ENOBUFS;
						return -1;
					};
					
					out = fp->pread(fp, tmpbuf, size, offset);
					vfsClose(fp);
					if (memcpy_k2u(buf, tmpbuf, size) != 0)
					{
						ERRNO = EFAULT;
						out = -1;
					};
					kfree(tmpbuf);
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
	case VFS_NO_MEMORY:
		ERRNO = ENOMEM;
		break;
	default:
		/* fallback in case there are some unhandled errors */
		getCurrentThread()->therrno = EIO;
		break;
	};
	return -1;
};

int sys_open(const char *upath, int oflag, mode_t mode)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	mode &= 0x0FFF;
	mode &= ~(getCurrentThread()->creds->umask);

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
			st.st_uid = getCurrentThread()->creds->euid;
			st.st_gid = getCurrentThread()->creds->egid;
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

	fp->oflag |= oflag;
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
	
	size_t argsize = (cmd >> 32) & 0xFFFF;
	void *argbuf = kmalloc(argsize);
	if (memcpy_u2k(argbuf, argp, argsize) != 0)
	{
		kfree(argbuf);
		vfsClose(fp);
		ERRNO = EFAULT;
		return -1;
	};
	
	int ret = fp->ioctl(fp, cmd, argbuf);
	memcpy_k2u(argp, argbuf, argsize);		// ignore error
	kfree(argbuf);
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

int sys_stat(const char *upath, struct stat *buf, size_t bufsz)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (bufsz > sizeof(struct stat))
	{
		bufsz = sizeof(struct stat);
	};

	struct stat kbuf;
	int status = vfsStat(path, &kbuf);
	if (status == 0)
	{
		if (memcpy_k2u(buf, &kbuf, bufsz) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
		
		return 0;
	}
	else
	{
		return sysOpenErrno(status);
	};
};

int sys_lstat(const char *upath, struct stat *buf, size_t bufsz)
{
	if (bufsz > sizeof(struct stat))
	{
		bufsz = sizeof(struct stat);
	};
	
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};

	struct stat kbuf;
	int status = vfsLinkStat(path, &kbuf);
	if (status == 0)
	{
		if (memcpy_k2u(buf, &kbuf, bufsz) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
		
		return 0;
	}
	else
	{
		return sysOpenErrno(status);
	};
};

int sys_pause()
{
	cli();
	lockSched();
	getCurrentThread()->therrno = EINTR;
	waitThread(getCurrentThread());
	unlockSched();
	kyield();
	return -1;
};

int sys_fstat(int fd, struct stat *buf, size_t bufsz)
{
	struct stat kbuf;
	
	if (bufsz > sizeof(struct stat))
	{
		bufsz = sizeof(struct stat);
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
	memset(&kbuf, 0, sizeof(struct stat));
	int status = fp->fstat(fp, &kbuf);
	vfsClose(fp);
	
	if (status == -1)
	{
		getCurrentThread()->therrno = EIO;
	};
	
	if (memcpy_k2u(buf, &kbuf, bufsz) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};

	return status;
};

int sys_chmod(const char *upath, mode_t mode)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
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

	if ((getCurrentThread()->creds->euid != 0) && (getCurrentThread()->creds->euid != dir->stat.st_uid))
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

	if ((getCurrentThread()->creds->euid != 0) && (getCurrentThread()->creds->euid != st.st_uid))
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
	if (ct->creds->euid == 0) return 1;

	if ((ct->creds->euid == uid) && (uid == st->st_uid))
	{
		return (ct->creds->egid == gid) || (ct->creds->sgid == gid) || (ct->creds->rgid == gid);
	};

	return 0;
};

int sys_chown(const char *upath, uid_t uid, gid_t gid)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
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

int sys_mkdir(const char *upath, mode_t mode)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	mode &= 0xFFF;
	mode &= ~(getCurrentThread()->creds->umask);

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
	if (dir == NULL)
	{
		vfsUnlockCreation();
		return sysOpenErrno(error);
	};
	
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

	int status = dir->mkdir(dir, newdir, mode & 0x0FFF, getCurrentThread()->creds->euid, getCurrentThread()->creds->egid);

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

int sys_unlink(const char *upath)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
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
		uid_t uid = getCurrentThread()->creds->euid;
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

int sys_insmod(const char *umodname, const char *upath, const char *uopt, int flags)
{
	char modname[USER_STRING_MAX];
	char path[USER_STRING_MAX];
	char bufopt[USER_STRING_MAX];
	char *opt;
	
	if (strcpy_u2k(modname, umodname) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (uopt != NULL)
	{
		if (strcpy_u2k(bufopt, uopt) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
		
		opt = bufopt;
	}
	else
	{
		opt = NULL;
	};
	
	if (getCurrentThread()->creds->euid != 0)
	{
		// only root can load modules.
		getCurrentThread()->therrno = EPERM;
		return -1;
	};

	return insmod(modname, path, opt, flags);
};

int sys_chdir(const char *upath)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
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
	if (size > 256) size = 256;
	if (memcpy_k2u(buf, getCurrentThread()->cwd, size) != 0)
	{
		ERRNO = EFAULT;
		return NULL;
	};
	return buf;
};

uint64_t sys_mmap(uint64_t addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	int allProt = PROT_READ | PROT_WRITE | PROT_EXEC;
	int allFlags = MAP_PRIVATE | MAP_SHARED | MAP_ANON | MAP_FIXED | MAP_THREAD;

	if ((offset & 0xFFF) != 0)
	{
		ERRNO = EINVAL;
		return MAP_FAILED;
	};
	
	if (prot == 0)
	{
		getCurrentThread()->therrno = EINVAL;
		return MAP_FAILED;
	};

	if ((prot & allProt) != prot)
	{
		getCurrentThread()->therrno = EINVAL;
		return MAP_FAILED;
	};
	
	if ((flags & allFlags) != flags)
	{
		getCurrentThread()->therrno = EINVAL;
		return MAP_FAILED;
	};

	if (flags & MAP_THREAD)
	{
		prot |= PROT_THREAD;
	};
	
	if ((flags & (MAP_PRIVATE | MAP_SHARED)) == 0)
	{
		getCurrentThread()->therrno = EINVAL;
		return MAP_FAILED;
	};

	if ((flags & MAP_PRIVATE) && (flags & MAP_SHARED))
	{
		getCurrentThread()->therrno = EINVAL;
		return MAP_FAILED;
	};

	if (len == 0)
	{
		getCurrentThread()->therrno = EINVAL;
		return MAP_FAILED;
	};

	if ((flags & MAP_FIXED) == 0)
	{
		addr = 0;
	}
	else if ((addr < 0x1000) || ((addr+len) > 0x7FC0000000))
	{
		ERRNO = EINVAL;
		return MAP_FAILED;
	}
	else if ((addr & 0xFFF) != 0)
	{
		// not page-aligned
		ERRNO = EINVAL;
		return MAP_FAILED;
	};

	if (flags & MAP_ANON)
	{
		if (fd != -1)
		{
			ERRNO = EINVAL;
			return MAP_FAILED;
		};
	}
	else if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		getCurrentThread()->therrno = EBADF;
		return MAP_FAILED;
	};

	if (flags & MAP_ANON)
	{
		if (flags & MAP_SHARED)
		{
			ERRNO = EINVAL;
			return MAP_FAILED;
		};
		
		int pageCount = (int) ((len + 0xFFF) & (~0xFFF)) >> 12; 
		FrameList *fl = palloc_later(NULL, pageCount, -1, 0);
		
		uint64_t outAddr;
		if (AddSegmentEx(getCurrentThread()->pm, addr/0x1000, fl, prot, &outAddr) != 0)
		{
			pdownref(fl);
			ERRNO = EINVAL;
			return MAP_FAILED;
		};
		
		pdownref(fl);
		
		return outAddr;
	}
	else
	{
		FileTable *ftab = getCurrentThread()->ftab;
		spinlockAcquire(&ftab->spinlock);

		File *fp = ftab->entries[fd];
		if ((fp->oflag & O_RDONLY) == 0)
		{
			spinlockRelease(&ftab->spinlock);
			getCurrentThread()->therrno = EACCES;
			return MAP_FAILED;
		};
		
		if (fp->mmap != NULL)
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			FrameList *fl = fp->mmap(fp, len, prot, flags, offset);
			vfsClose(fp);
			if (fl == NULL)
			{
				getCurrentThread()->therrno = ENODEV;
				return MAP_FAILED;
			};

			uint64_t outAddr;
			if (AddSegmentEx(getCurrentThread()->pm, addr/0x1000, fl, prot, &outAddr) != 0)
			{
				pdownref(fl);
				getCurrentThread()->therrno = EINVAL;
				return MAP_FAILED;
			};

			pdownref(fl);

			return outAddr;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			
			if (flags & MAP_SHARED)
			{
				// TODO: not supported!
				vfsClose(fp);
				ERRNO = EINVAL;
				return MAP_FAILED;
			};
			
			int pageCount = (int) ((len + 0xFFF) & (~0xFFF)) >> 12; 
			FrameList *fl = palloc_later(fp, pageCount, offset, len);
			vfsClose(fp);	// already dupped by palloc_later
			
			uint64_t outAddr;
			if (AddSegmentEx(getCurrentThread()->pm, addr/0x1000, fl, prot, &outAddr) != 0)
			{
				pdownref(fl);
				ERRNO = EINVAL;
				return MAP_FAILED;
			};
			
			pdownref(fl);
			
			return outAddr;
		};
	};
};

int setuid(uid_t uid)
{
	Thread *me = getCurrentThread();
	if ((me->creds->euid != 0) && (uid != me->creds->euid) && (uid != me->creds->ruid))
	{
		me->therrno = EPERM;
		return -1;
	};

	if (me->creds->euid == 0)
	{
		me->creds->ruid = uid;
		me->creds->suid = uid;
	};

	me->creds->euid = uid;
	return 0;
};

int setgid(gid_t gid)
{
	Thread *me = getCurrentThread();
	if ((me->creds->egid != 0) && (gid != me->creds->egid) && (gid != me->creds->rgid))
	{
		me->therrno = EPERM;
		return -1;
	};

	if (me->creds->egid == 0)
	{
		me->creds->rgid = gid;
		me->creds->sgid = gid;
	};

	me->creds->egid = gid;
	return 0;
};

int setreuid(uid_t ruid, uid_t euid)
{
	Thread *me = getCurrentThread();
	uid_t ruidPrev = me->creds->ruid;
	if (ruid != (uid_t)-1)
	{
		if (me->creds->euid != 0)
		{
			me->therrno = EPERM;
			return -1;
		};

		me->creds->ruid = ruid;
	};

	if (euid != (uid_t)-1)
	{
		if (euid != ruidPrev)
		{
			if (me->creds->euid != 0)
			{
				me->therrno = EPERM;
				return -1;
			};

			me->creds->suid = euid;
		};

		me->creds->euid = euid;
	};

	return 0;
};

int setregid(gid_t rgid, gid_t egid)
{
	Thread *me = getCurrentThread();
	gid_t rgidPrev = me->creds->rgid;
	if (rgid != (gid_t)-1)
	{
		if (me->creds->egid != 0)
		{
			me->therrno = EPERM;
			return -1;
		};

		me->creds->rgid = rgid;
	};

	if (egid != (gid_t)-1)
	{
		if (egid != rgidPrev)
		{
			if (me->creds->egid != 0)
			{
				me->therrno = EPERM;
				return -1;
			};

			me->creds->sgid = egid;
		};

		me->creds->egid = egid;
	};

	return 0;
};

int seteuid(uid_t euid)
{
	Thread *me = getCurrentThread();
	if (me->creds->euid != 0)
	{
		if ((euid != me->creds->euid) && (euid != me->creds->ruid) && (euid != me->creds->suid))
		{
			me->therrno = EPERM;
			return -1;
		};
	};

	me->creds->euid = euid;
	return 0;
};

int setegid(gid_t egid)
{
	Thread *me = getCurrentThread();
	if (me->creds->egid != 0)
	{
		if ((egid != me->creds->egid) && (egid != me->creds->rgid) && (egid != me->creds->sgid))
		{
			me->therrno = EPERM;
			return -1;
		};
	};

	me->creds->egid = egid;
	return 0;
};

int sys_rmmod(const char *umodname, int flags)
{
	char modname[USER_STRING_MAX];
	if (strcpy_u2k(modname, umodname) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	// only root can remove modules!
	if (getCurrentThread()->creds->euid != 0)
	{
		getCurrentThread()->therrno = EPERM;
		return -1;
	};

	return rmmod(modname, flags);
};

int sys_link(const char *uoldname, const char *unewname)
{
	char oldname[USER_STRING_MAX];
	if (strcpy_u2k(oldname, uoldname) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	char newname[USER_STRING_MAX];
	if (strcpy_u2k(newname, unewname) != 0)
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

int sys_symlink(const char *utarget, const char *upath)
{
	char target[USER_STRING_MAX];
	if (strcpy_u2k(target, utarget) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
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

ssize_t sys_readlink(const char *upath, char *buf, size_t bufsize)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
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

	if (memcpy_k2u(buf, tmpbuf, bufsize) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return out;
};

unsigned sys_sleep(unsigned seconds)
{
	if (seconds == 0) return 0;

	//cli();
	//uint64_t sleepStart = (uint64_t) getTicks();
	//uint64_t wakeTime = ((uint64_t) getTicks() + (uint64_t) seconds * (uint64_t) 1000);
	//getCurrentThread()->wakeTime = wakeTime;
	//getCurrentThread()->flags |= THREAD_WAITING;
	//waitThread(getCurrentThread());
	//kyield();
#if 0
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
#endif

	cli();
	lockSched();
	
	uint64_t sleepStart = getNanotime();
	uint64_t wakeTime = sleepStart + (uint64_t)seconds * NANO_PER_SEC;
	
	TimedEvent ev;
	timedPost(&ev, wakeTime);
	
	while (getNanotime() <= wakeTime)
	{
		waitThread(getCurrentThread());
		unlockSched();
		kyield();
		
		cli();
		lockSched();
		if (haveReadySigs(getCurrentThread())) break;
	};
	
	timedCancel(&ev);
	unlockSched();
	sti();
	
	uint64_t now = getNanotime();
	if (now > wakeTime)
	{
		return 0;
	};
	
	uint64_t timeLeft = wakeTime - now;
	unsigned secondsLeft = (unsigned) (timeLeft / NANO_PER_SEC) + 1;
	return secondsLeft;
};

int sys_utime(const char *upath, time_t atime, time_t mtime)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
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
		if (dirp->stat.st_uid != getCurrentThread()->creds->euid)
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
	mode_t old = getCurrentThread()->creds->umask;
	getCurrentThread()->creds->umask = (cmask & 0777);
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

int sys_bind(int fd, const struct sockaddr *uaddr, size_t addrlen)
{
	struct sockaddr addr;
	if (addrlen > sizeof(struct sockaddr)) addrlen = sizeof(struct sockaddr);
	
	if (memcpy_u2k(&addr, uaddr, addrlen) != 0)
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
			out = BindSocket(fp, &addr, addrlen);
			vfsClose(fp);
		};
	};

	return out;
};

ssize_t sys_sendto(int fd, const void *umessage, size_t len, int flags, const struct sockaddr *uaddr, size_t addrlen)
{
	void *message = kmalloc(len);
	if (memcpy_u2k(message, umessage, len) != 0)
	{
		kfree(message);
		ERRNO = EFAULT;
		return -1;
	};
	
	if (addrlen > sizeof(struct sockaddr)) addrlen = sizeof(struct sockaddr);
	
	struct sockaddr addr;
	memset(&addr, 0, sizeof(struct sockaddr));
	if (memcpy_u2k(&addr, uaddr, addrlen) != 0)
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
			out = SendtoSocket(fp, message, len, flags, &addr, addrlen);
			vfsClose(fp);
		};
	};

	kfree(message);
	return out;
};

ssize_t sys_recvfrom(int fd, void *umessage, size_t len, int flags, struct sockaddr *uaddr, size_t *uaddrlen)
{
	void *message = kmalloc(len);
	struct sockaddr kaddr;
	struct sockaddr *addr = NULL;
	size_t kaddrlen;
	size_t *addrlen = NULL;
	
	if (uaddr != NULL)
	{
		if (memcpy_u2k(&kaddr, uaddr, sizeof(struct sockaddr)) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
		
		addr = &kaddr;
	};
	
	if (uaddrlen != NULL)
	{
		if (memcpy_u2k(&kaddrlen, uaddrlen, sizeof(size_t)) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
		
		addrlen = &kaddrlen;
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

	if (umessage != NULL) memcpy_k2u(umessage, message, len);
	if (uaddr != NULL)
	{
		if (addrlen != NULL)
		{
			memcpy_k2u(uaddr, addr, *addrlen);
		};
	};
	if (uaddrlen != NULL) memcpy_k2u(uaddrlen, addrlen, sizeof(size_t));
	
	return out;
};

int sys_getsockname(int fd, struct sockaddr *uaddr, size_t *uaddrlenptr)
{
	struct sockaddr addr;
	size_t addrlen;
	
	if (memcpy_u2k(&addrlen, uaddrlenptr, sizeof(size_t)) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (addrlen > sizeof(struct sockaddr)) addrlen = sizeof(struct sockaddr);
	
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
			out = SocketGetsockname(fp, &addr, &addrlen);
			vfsClose(fp);
		};
	};

	memcpy_k2u(uaddr, &addr, addrlen);
	memcpy_k2u(uaddrlenptr, &addrlen, sizeof(size_t));
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

int sys_connect(int fd, struct sockaddr *uaddr, size_t addrlen)
{
	struct sockaddr kaddr;
	if (addrlen > sizeof(struct sockaddr)) addrlen = sizeof(struct sockaddr);
	if (memcpy_u2k(&kaddr, uaddr, addrlen) != 0)
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
			out = ConnectSocket(fp, &kaddr, addrlen);
			vfsClose(fp);
		};
	};

	return out;
};

int sys_getpeername(int fd, struct sockaddr *uaddr, size_t *uaddrlenptr)
{
	struct sockaddr addr;
	size_t addrlen;
	
	if (memcpy_u2k(&addrlen, uaddrlenptr, sizeof(size_t)) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (addrlen > sizeof(struct sockaddr)) addrlen = sizeof(struct sockaddr);
		
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
			out = SocketGetpeername(fp, &addr, &addrlen);
			vfsClose(fp);
		};
	};

	memcpy_k2u(uaddr, &addr, addrlen);
	memcpy_k2u(uaddrlenptr, &addrlen, sizeof(size_t));
	
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

int sys_uname(struct utsname *ubuf)
{
	struct utsname buf;
	strcpy(buf.sysname, UNAME_SYSNAME);
	strcpy(buf.nodename, UNAME_NODENAME);
	strcpy(buf.release, UNAME_RELEASE);
	strcpy(buf.version, UNAME_VERSION);
	strcpy(buf.machine, UNAME_MACHINE);
	
	if (memcpy_k2u(ubuf, &buf, sizeof(struct utsname)) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
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
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		getCurrentThread()->therrno = EBADF;
		return 0;
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
			return 0;
		}
		else
		{
			ERRNO = ENOTTY;
			int result = !!(fp->oflag & O_TERMINAL);
			spinlockRelease(&ftab->spinlock);
			return result;
		};
	};
};

int sys_bindif(int fd, const char *uifname)
{
	char ifname[USER_STRING_MAX];
	if (strcpy_u2k(ifname, uifname) != 0)
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

int sys_store_and_sleep(uint8_t *ptr, uint8_t value)
{
	uint64_t ptrAddr = (uint64_t) ptr;
	if (ptrAddr >= 0x7FC0000000)
	{
		return EFAULT;
	};
	
	cli();
	lockSched();
	
	waitThread(getCurrentThread());
	unlockSched();
	
	// interrupts are still disabled, so we can't be preempted before doing the store,
	// even if another CPU has already woken us up, and since the scheduler is unlocked,
	// we can safely access userland memory.
	if (catch() == 0)
	{
		*ptr = value;
		__sync_synchronize();
	}
	else
	{
		sti();
		return EFAULT;
	};
	
	kyield();
	return 0;
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

int sys_exec(const char *upath, const char *upars, uint64_t parsz)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	char pars[4096];
	if (parsz > 4096)
	{
		ERRNO = EOVERFLOW;
		return -1;
	};
	
	if (memcpy_u2k(pars, upars, parsz) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return elfExec(path, pars, parsz);
};

int sys_getpid()
{
	return getCurrentThread()->creds->pid;
};

uid_t sys_getuid()
{
	return getCurrentThread()->creds->ruid;
};

uid_t sys_geteuid()
{
	return getCurrentThread()->creds->euid;
};

uid_t sys_getsuid()
{
	return getCurrentThread()->creds->suid;
};

gid_t sys_getgid()
{
	return getCurrentThread()->creds->rgid;
};

gid_t sys_getegid()
{
	return getCurrentThread()->creds->egid;
};

gid_t sys_getsgid()
{
	return getCurrentThread()->creds->sgid;
};

int sys_sigaction(int sig, const SigAction *uact, SigAction *uoact)
{
	SigAction act;
	if (uact != NULL)
	{
		if (memcpy_u2k(&act, uact, sizeof(SigAction)) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
	};
	
	if ((sig < 1) || (sig >= SIG_NUM))
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if ((sig == SIGSTOP) || (sig == SIGKILL))
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	SigAction oact;
	
	cli();
	lockSched();
	memcpy(&oact, &getCurrentThread()->sigdisp->actions[sig], sizeof(SigAction));
	
	if (uact != NULL)
	{
		memcpy(&getCurrentThread()->sigdisp->actions[sig], &act, sizeof(SigAction));
	};
	
	unlockSched();
	sti();
	
	if (uoact != NULL)
	{
		if (memcpy_k2u(uoact, &oact, sizeof(SigAction)) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
	};
	
	return 0;
};

int sys_sigprocmask(int how, uint64_t *set, uint64_t *oldset)
{
	uint64_t newset;
	if (set != NULL)
	{
		if (memcpy_u2k(&newset, set, sizeof(uint64_t)) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
	};
	
	if (oldset != NULL)
	{	
		if (memcpy_k2u(oldset, &getCurrentThread()->sigmask, sizeof(uint64_t)) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
	};
	
	if (set != NULL)
	{
		if (how == SIG_BLOCK)
		{
			getCurrentThread()->sigmask |= newset;
		}
		else if (how == SIG_UNBLOCK)
		{
			getCurrentThread()->sigmask &= ~newset;
		}
		else if (how == SIG_SETMASK)
		{
			getCurrentThread()->sigmask = newset;
		}
		else
		{
			ERRNO = EINVAL;
			return -1;
		};
	};
	
	return 0;
};

uint64_t sys_getparsz()
{
	return getCurrentThread()->szExecPars;
};

void sys_getpars(void *buffer, size_t size)
{
	if (size > getCurrentThread()->szExecPars)
	{
		size = getCurrentThread()->szExecPars;
	};
	memcpy_k2u(buffer, getCurrentThread()->execPars, size);
};

int sys_geterrno()
{
	return ERRNO;
};

void sys_seterrno(int num)
{
	ERRNO = num;
};

int sys_fork()
{	
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
	return threadClone(&regs, 0, NULL);
};

int sys_waitpid(int pid, int *stat_loc, int flags)
{	
	int statret;
	int ret = processWait(pid, &statret, flags);
	if (stat_loc != NULL) memcpy_k2u(stat_loc, &statret, sizeof(int));
	return ret;
};

void sys_yield()
{
	kyield();
};

char* sys_realpath(const char *upath, char *ubuffer)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return NULL;
	};
	
	char buffer[256];
	
	ERRNO = ENAMETOOLONG;
	char *result = realpath(path, buffer);
	if (result == NULL) return NULL;
	
	memcpy_k2u(ubuffer, buffer, 256);
	return ubuffer;
};

void sys_seterrnoptr(int *ptr)
{
	getCurrentThread()->errnoptr = ptr;
};

int* sys_geterrnoptr()
{
	return getCurrentThread()->errnoptr;
};

int sys_unmount(const char *upath)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
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
	gen_route route;
	if (memcpy_u2k(&route, c, sizeof(gen_route)) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return route_add(a, b, &route);
};

ssize_t sys_netconf_stat(const char *ua, NetStat *b, size_t c)
{
	char a[USER_STRING_MAX];
	if (strcpy_u2k(a, ua) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	NetStat res;
	if (c > sizeof(NetStat)) c = sizeof(NetStat);
	ssize_t result = netconf_stat(a, &res, c);
	memcpy_k2u(b, &res, c);
	return result;
};

ssize_t sys_netconf_getaddrs(const char *ua, int b, void *c, size_t d)
{
	char a[USER_STRING_MAX];
	if (strcpy_u2k(a, ua) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	void *buf = kmalloc(d);
	ssize_t result = netconf_getaddrs(a, b, buf, d);
	memcpy_k2u(c, buf, d);
	kfree(buf);
	return result;
};

ssize_t sys_netconf_statidx(unsigned int a, NetStat *b, size_t c)
{
	NetStat res;
	if (c > sizeof(NetStat)) c = sizeof(NetStat);
	ssize_t result = netconf_statidx(a, &res, c);
	memcpy_k2u(b, &res, c);
	return result;
};

int sys_getgroups(int count, gid_t *buffer)
{
	if (count <= getCurrentThread()->creds->numGroups)
	{
		if (memcpy_k2u(buffer, getCurrentThread()->creds->groups, sizeof(gid_t)*count) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
	}
	else
	{
		if (memcpy_k2u(buffer, getCurrentThread()->creds->groups, sizeof(gid_t)*getCurrentThread()->creds->numGroups) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
	};
	return getCurrentThread()->creds->numGroups;
};

int sys_setgroups(int count, const gid_t *groups)
{
	if (getCurrentThread()->creds->egid != 0)
	{
		ERRNO = EPERM;
		return -1;
	};
	if (count > 16)
	{
		ERRNO = EINVAL;
		return -1;
	};
	if (memcpy_u2k(getCurrentThread()->creds->groups, groups, sizeof(gid_t)*count) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	getCurrentThread()->creds->numGroups = count;
	return 0;
};

int sys_netconf_addr(int a, const char *ub, void *c, uint64_t d)
{
	char b[USER_STRING_MAX];
	if (strcpy_u2k(b, ub) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	void *buf = NULL;
	if (c != NULL)
	{
		buf = kmalloc(d);
		if (memcpy_u2k(buf, c, d) != 0)
		{
			kfree(buf);
			ERRNO = EFAULT;
			return -1;
		};
	}
	else if (d != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int result = netconf_addr(a, b, buf, d);
	kfree(buf);
	return result;
};

int sys_route_clear(int a, const char *ub)
{
	char b[USER_STRING_MAX];
	if (strcpy_u2k(b, ub) != 0)
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
	return getCurrentThread()->creds->ppid;
};

int sys_setsid()
{
	cli();
	lockSched();
	
	// fail with EPERM if we are a process group leader
	if (getCurrentThread()->creds->pid == getCurrentThread()->creds->pgid)
	{
		unlockSched();
		sti();
		ERRNO = EPERM;
		return -1;
	};
	
	Thread *me = getCurrentThread();
	int result = me->creds->pid;
	me->creds->sid = me->creds->pid;
	me->creds->pgid = me->creds->pid;
	
	unlockSched();
	sti();
	
	return result;
};

int sys_setpgid(int pid, int pgid)
{
	cli();
	lockSched();
	
	Thread *target = NULL;
	if (pid == 0)
	{
		target = getCurrentThread();
	}
	else
	{
		target = getThreadByPID(pid);
	};
	
	if (target == NULL)
	{
		unlockSched();
		sti();
		ERRNO = ESRCH;
		return -1;
	};
	
	if (target->creds->ppid != getCurrentThread()->creds->pid)
	{
		if (target != getCurrentThread())
		{
			unlockSched();
			sti();
			ERRNO = ESRCH;
			return -1;
		};
	};
	
	if (target->creds->sid == target->creds->pid)
	{
		unlockSched();
		sti();
		ERRNO = EPERM;
		return -1;
	};
	
	if (pgid == 0) pgid = target->creds->pid;
	
	if (pgid != target->creds->pid)
	{
		// find a prototype thread which is already part of the group we are
		// trying to join, and make sure it's in the same session.
		Thread *scan = getCurrentThread();
		Thread *ex = NULL;
		
		do
		{
			if (scan->creds->pgid == pgid)
			{
				ex = scan;
				break;
			};
			
			scan = scan->next;
		} while (scan != getCurrentThread());
		
		if (ex == NULL)
		{
			// the group does not exist
			unlockSched();
			sti();
			ERRNO = EPERM;
			return -1;
		};
		
		if (ex->creds->sid != target->creds->sid)
		{
			// different session; can't join
			unlockSched();
			sti();
			ERRNO = EPERM;
			return -1;
		};
	};
	
	target->creds->pgid = pgid;
	unlockSched();
	sti();
	return 0;
};

int sys_getsid(int pid)
{
	if (pid == 0) return getCurrentThread()->creds->sid;
	
	cli();
	lockSched();
	Thread *target = getThreadByPID(pid);
	int result;
	if (target == NULL)
	{
		ERRNO = ESRCH;
		result = -1;
	}
	else
	{
		result = target->creds->sid;
	};
	unlockSched();
	sti();
	
	return result;
};

int sys_getpgid(int pid)
{
	if (pid == 0) return getCurrentThread()->creds->pgid;
	
	int result;
	
	cli();
	lockSched();
	Thread *target = getThreadByPID(pid);
	if (target == NULL)
	{
		ERRNO = ESRCH;
		result = -1;
	}
	else
	{
		result = target->creds->pgid;
	};
	unlockSched();
	sti();
	
	return result;
};

int sys_diag(void *uptr)
{
	char buffer[30];
	return memcpy_u2k(buffer, uptr, 30);
};

void sys_pthread_exit(uint64_t retval)
{
	threadExitEx(retval);
};

extern char usup_start;
extern char usup_thread_entry;

int sys_pthread_create(int *thidOut, const ThreadAttr *uattr, uint64_t entry, uint64_t arg)
{
	ThreadAttr attr;
	if (uattr == NULL)
	{
		attr.scope = 0;
		attr.detachstate = 0;
		attr.inheritsched = 0;
		attr.stack = NULL;
		attr.stacksize = 0x200000;			// 2MB
	}
	else
	{
		if (memcpy_u2k(&attr, uattr, 256) != 0)
		{
			return EFAULT;
		};
	};
	
	if (attr.scope != 0)
	{
		return EINVAL;
	};
	
	if ((attr.detachstate != 0) && (attr.detachstate != 1))
	{
		return EINVAL;
	};
	
	if (attr.inheritsched != 0)
	{
		return EINVAL;
	};
	
	if (attr.stacksize < 0x1000)
	{
		return EINVAL;
	};
	
	// set up registers such that usup_thread_entry() in the user support page is
	// entered, passing approripate information to it.
	Regs regs;
	initUserRegs(&regs);
	regs.rip = (uint64_t)(&usup_thread_entry) - (uint64_t)(&usup_start) + 0xFFFF808000003000UL;
	regs.rbx = 0;
	regs.r12 = 0;
	regs.r14 = entry;
	regs.r15 = arg;
	regs.rbp = 0;
	
	if (attr.stack == NULL)
	{
		int stackPages = (attr.stacksize + 0xFFF) >> 12;
		FrameList *fl = palloc_later(NULL, stackPages, -1, 0);
		if (AddSegmentEx(getCurrentThread()->pm, 0, fl, PROT_READ | PROT_WRITE | PROT_THREAD, &regs.rbx) != 0)
		{
			pdownref(fl);
			return EAGAIN;
		};
		
		pdownref(fl);
		regs.r12 = stackPages << 12;
		regs.rsp = regs.rbx + regs.r12;
	}
	else
	{
		regs.rsp = (uint64_t) attr.stack + attr.stacksize;
	};
	
	// create the thread
	int cloneFlags = CLONE_THREAD;
	if (attr.detachstate == 1)		// detached
	{
		cloneFlags |= CLONE_DETACHED;
	};
	
	int thid = threadClone(&regs, cloneFlags, NULL);
	
	if (memcpy_k2u(thidOut, &thid, sizeof(int)) != 0)
	{
		// at this point the program state is undefined since it lost control
		// of the thread it just spawned
		return EFAULT;
	};
	
	return 0;
};

int sys_pthread_self()
{
	return getCurrentThread()->thid;
};

extern char usup_syscall_reset;

int sys_pthread_join(int thid, uint64_t *retval)
{
	uint64_t kretval;
	int result = joinThread(thid, &kretval);
	
	if (result == 0)
	{
		if (memcpy_k2u(retval, &kretval, 8) != 0)
		{
			return EFAULT;
		};
		
		return 0;
	}
	else if (result == -1)
	{
		return EDEADLK;
	}
	else if (result == -2)
	{
		Thread *me = getCurrentThread();
		Regs regs;
		initUserRegs(&regs);
		
		// preserved registers must be there
		regs.rbx = me->urbx;
		regs.rbp = me->urbp;
		regs.rsp = me->ursp;
		regs.r12 = me->ur12;
		regs.r13 = me->ur13;
		regs.r14 = me->ur14;
		regs.r15 = me->ur15;
		regs.r9 = me->urip;			// usup_syscall_reset() wants return RIP in R9
		regs.rflags = getFlagsRegister();
		regs.rdi = 0;
		
		// make sure we retry using the same argumets
		*((int*)&regs.rdi) = thid;
		regs.rsi = (uint64_t) retval;		// the retval POINTER
		regs.rip = (uint64_t)(&usup_syscall_reset) - (uint64_t)(&usup_start) + 0xFFFF808000003000UL;
		
		// and make sure we call pthread_join()
		regs.rax = 117;
		
		switchTask(&regs);
	};
	
	return EDEADLK;		// shouldn't get here
};

int sys_pthread_detach(int thid)
{
	return detachThread(thid);
};

int sys_pthread_kill(int thid, int sig)
{
	return signalThid(thid, sig);
};

int sys_kopt(int option, int value)
{
	// only root is allowed to change kernel options
	if (getCurrentThread()->creds->euid != 0)
	{
		ERRNO = EPERM;
		return -1;
	};
	
	if (option == _GLIDIX_KOPT_GFXTERM)
	{
		setGfxTerm(value);
		return 0;
	}
	else
	{
		ERRNO = EINVAL;
		return -1;
	};
};

int sys_sigwait(uint64_t set, siginfo_t *infoOut, uint64_t nanotimeout)
{
	uint64_t deadline = getNanotime() + nanotimeout;
	if (nanotimeout == 0xFFFFFFFFFFFFFFFF)
	{
		deadline = 0;
	};
	
	// never wait for certain signals
	set &= ~((1UL << SIGKILL) | (1UL << SIGSTOP) | (1UL << SIGTHKILL) | (1UL << SIGTHWAKE));
	
	cli();
	lockSched();
	
	TimedEvent ev;
	timedPost(&ev, deadline);
	
	Thread *me = getCurrentThread();
	
	while (1)
	{
		int i;
		for (i=0; i<64; i++)
		{
			if (me->pendingSet & (1UL << i))
			{
				uint64_t sigbit = (1UL << me->pendingSigs[i].si_signo);
				if (set & sigbit)
				{
					// one of the signals we are waiting for is pending!
					timedCancel(&ev);
					siginfo_t si;
					memcpy(&si, &me->pendingSigs[i].si_signo, sizeof(siginfo_t));
					me->pendingSet &= ~(1UL << i);
					unlockSched();
					sti();
					
					if (infoOut != NULL)
					{
						if (memcpy_k2u(infoOut, &si, sizeof(siginfo_t)) != 0)
						{
							return EFAULT;
						};
					};
					
					return 0;
				};
				
				if (((me->sigmask & sigbit) == 0) || isUnblockableSig(me->pendingSigs[i].si_signo))
				{
					// we're not waiting for it, and it's not blocked, so we were interrupted
					timedCancel(&ev);
					unlockSched();
					sti();
					
					return EINTR;
				};
			};
		};
		
		if ((getNanotime() >= deadline) && (deadline != 0))
		{
			timedCancel(&ev);
			unlockSched();
			sti();
			
			return EAGAIN;
		};
		
		unlockSched();
		kyield();
		
		cli();
		lockSched();
	};
};

/**
 * NOTE: This is called _glidix_sigsuspend() in userspace because it takes the set
 * as a direct argument instead of a pointer, to avoid the hassle of a user-to-kernel
 * copy.
 */
int sys_sigsuspend(uint64_t mask)
{
	uint64_t oldmask = atomic_swap64(&getCurrentThread()->sigmask, mask);
	
	while (!wasSignalled())
	{
		cli();
		lockSched();
		waitThread(getCurrentThread());
		unlockSched();
		sti();
	};
	
	getCurrentThread()->sigmask = oldmask;
	ERRNO = EINTR;
	return -1;
};

int sys_lockf(int fd, int cmd, off_t len)
{
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		ERRNO = EBADF;
		return -1;
	};
	
	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);
	File *fp = ftab->entries[fd];
	if (fp == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		ERRNO = EBADF;
		return -1;
	};
	
	vfsDup(fp);
	spinlockRelease(&ftab->spinlock);
	
	if ((cmd == F_LOCK) || (cmd == F_TLOCK))
	{
		if ((fp->oflag & O_WRONLY) == 0)
		{
			// file not writeable, can't lock
			vfsClose(fp);
			ERRNO = EBADF;
			return -1;
		};
	};
	
	off_t pos = 0;
	if (fp->seek != NULL)
	{
		pos = fp->seek(fp, 0, SEEK_CUR);
	};
	
	struct stat st;
	if (fp->fstat == NULL)
	{
		vfsClose(fp);
		ERRNO = EOPNOTSUPP;
		return -1;
	};
	
	if (fp->fstat(fp, &st) != 0)
	{
		vfsClose(fp);
		ERRNO = EIO;
		return -1;
	};
	
	int error = vfsFileLock(fp, cmd, st.st_dev, st.st_ino, pos, len);
	vfsClose(fp);
	
	ERRNO = error;
	if (error != 0) return -1;
	return 0;
};

int sys_mcast(int fd, int op, uint32_t scope, uint64_t addr0, uint64_t addr1)
{
	struct in6_addr addr;
	uint64_t *addrptr = (uint64_t*) &addr;
	addrptr[0] = addr0;
	addrptr[1] = addr1;
	
	if (addr.s6_addr[0] != 0xFF)
	{
		// not multicast
		ERRNO = EINVAL;
		return -1;
	};
	
	if ((op != MCAST_JOIN_GROUP) && (op != MCAST_LEAVE_GROUP))
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	int out;
	if ((fd >= MAX_OPEN_FILES) || (fd < 0))
	{
		ERRNO = EBADF;
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
			ERRNO = EBADF;
			out = -1;
		}
		else
		{
			vfsDup(fp);
			spinlockRelease(&ftab->spinlock);
			out = SocketMulticast(fp, op, &addr, scope);
			vfsClose(fp);
		};
	};

	return out;
};

int sys_fpoll(const uint8_t *ubitmapReq, uint8_t *ubitmapRes, int flags, uint64_t nanotimeout)
{
	uint8_t bitmapReq[MAX_OPEN_FILES];
	uint8_t bitmapRes[MAX_OPEN_FILES];
	
	if (memcpy_u2k(bitmapReq, ubitmapReq, MAX_OPEN_FILES) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	memset(bitmapRes, 0, MAX_OPEN_FILES);
	
	Semaphore *sems[MAX_OPEN_FILES*8];
	memset(sems, 0, MAX_OPEN_FILES*8*sizeof(void*));
	
	File* workingFiles[MAX_OPEN_FILES];
	memset(workingFiles, 0, MAX_OPEN_FILES*sizeof(File*));
	
	// get the file handles
	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);
	
	int i;
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		if (bitmapReq[i] != 0)
		{
			File *fp = ftab->entries[i];
			if (fp == NULL)
			{
				sems[8*i+PEI_INVALID] = vfsGetConstSem();
			}
			else
			{
				vfsDup(fp);
				workingFiles[i] = fp;
			};
		};
	};
	
	spinlockRelease(&ftab->spinlock);
	
	// ask all files for their poll information.
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		if (workingFiles[i] != NULL)
		{
			if (workingFiles[i]->pollinfo != NULL)
			{
				workingFiles[i]->pollinfo(workingFiles[i], &sems[8*i]);
				
				// clear the entries which we don't want to occur
				uint8_t pollFor = bitmapReq[i] | POLL_ERROR | POLL_INVALID | POLL_HANGUP;
				
				int j;
				for (j=0; j<8; j++)
				{
					if ((pollFor & (1 << j)) == 0)
					{
						sems[8*i+j] = NULL;
					};
				};
			};
		};
	};
	
	// wait for something to happen
	int numFreeSems = semPoll(8*MAX_OPEN_FILES, sems, bitmapRes, SEM_W_FILE(flags), nanotimeout);
	
	// remove our references to the files
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		if (workingFiles[i] != NULL)
		{
			vfsClose(workingFiles[i]);
		};
	};
	
	// see if an error occured
	if (numFreeSems < 0)
	{
		ERRNO = -numFreeSems;
		return -1;
	};
	
	if (memcpy_k2u(ubitmapRes, bitmapRes, MAX_OPEN_FILES) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return numFreeSems;
};

uint64_t sys_oxperm()
{
	return getCurrentThread()->oxperm;
};

uint64_t sys_dxperm()
{
	return getCurrentThread()->dxperm;
};

size_t sys_fsinfo(FSInfo *ulist, size_t count)
{
	if (count > 256) count = 256;
	
	FSInfo list[256];
	memset(list, 0, sizeof(FSInfo)*256);
	
	size_t result = getFSInfo(list, count);
	
	if (memcpy_k2u(ulist, list, sizeof(FSInfo)*256) != 0)
	{
		ERRNO = EFAULT;
		return 0;
	};
	
	return result;
};

int sys_chxperm(const char *upath, uint64_t ixperm, uint64_t oxperm, uint64_t dxperm)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};

	if (!havePerm(XP_CHXPERM))
	{
		ERRNO = EPERM;
		return -1;
	};
	
	if (ixperm != XP_NCHG)
	{
		if ((getCurrentThread()->dxperm & ixperm) != ixperm)
		{
			ERRNO = EPERM;
			return -1;
		};
	};

	if (oxperm != XP_NCHG)
	{
		if ((getCurrentThread()->dxperm & oxperm) != oxperm)
		{
			ERRNO = EPERM;
			return -1;
		};
	};

	if (dxperm != XP_NCHG)
	{
		if ((getCurrentThread()->dxperm & dxperm) != dxperm)
		{
			ERRNO = EPERM;
			return -1;
		};
	};

	int error;
	Dir *dir = parsePath(path, VFS_CHECK_ACCESS, &error);

	if (dir == NULL)
	{
		return sysOpenErrno(error);
	};
	
	if ((dir->stat.st_mode & 0xF000) != 0)
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		ERRNO = EINVAL;
		return -1;
	};
	
	if (dir->chxperm == NULL)
	{
		if (dir->close != NULL) dir->close(dir);
		kfree(dir);
		ERRNO = EIO;
		return -1;
	};

	int status = dir->chxperm(dir, ixperm, oxperm, dxperm);
	if (dir->close != NULL) dir->close(dir);
	kfree(dir);

	if (status != 0) ERRNO = EIO;
	return status;
};

/**
 * System call table for fast syscalls, and the number of system calls.
 */
#define SYSCALL_NUMBER 130
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
	&sys_sigaction,				// 13
	&sys_sigprocmask,			// 14
	&sys_stat,				// 15
	&sys_getparsz,				// 16
	&sys_getpars,				// 17
	&sys_raise,				// 18
	&sys_geterrno,				// 19
	&sys_seterrno,				// 20
	&mprotect,				// 21
	&sys_lseek,				// 22
	&sys_fork,				// 23
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
	&sys_pread,				// 52
	&sys_pwrite,				// 53
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
	&sys_store_and_sleep,			// 103
	&sys_mqserver,				// 104
	&sys_mqclient,				// 105
	&sys_mqsend,				// 106
	&sys_mqrecv,				// 107
	&sys_shmalloc,				// 108
	&sys_shmap,				// 109
	&sys_setsid,				// 110
	&sys_setpgid,				// 111
	&sys_getsid,				// 112
	&sys_getpgid,				// 113
	&sys_pthread_exit,			// 114
	&sys_pthread_create,			// 115
	&sys_pthread_self,			// 116
	&sys_pthread_join,			// 117
	&sys_pthread_detach,			// 118
	&sys_pthread_kill,			// 119
	&sys_kopt,				// 120
	&sys_sigwait,				// 121
	&sys_sigsuspend,			// 122
	&sys_lockf,				// 123
	&sys_mcast,				// 124
	&sys_fpoll,				// 125
	&sys_oxperm,				// 126
	&sys_dxperm,				// 127
	&sys_fsinfo,				// 128
	&sys_chxperm,				// 129
};
uint64_t sysNumber = SYSCALL_NUMBER;

uint64_t sysEpilog(uint64_t retval)
{
	if (ERRNO != 0)
	{
		if (getCurrentThread()->errnoptr != NULL)
		{
			if (catch() == 0)
			{
				*(getCurrentThread()->errnoptr) = getCurrentThread()->therrno;
				uncatch();
			};
		};
	};

	if (wasSignalled())
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
