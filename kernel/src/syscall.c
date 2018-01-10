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
#include <glidix/fsdriver.h>
#include <glidix/time.h>
#include <glidix/pipe.h>
#include <glidix/down.h>
#include <glidix/netif.h>
#include <glidix/socket.h>
#include <glidix/pci.h>
#include <glidix/utsname.h>
#include <glidix/catch.h>
#include <glidix/storage.h>
#include <glidix/cpu.h>
#include <glidix/pageinfo.h>
#include <glidix/msr.h>
#include <glidix/trace.h>

/**
 * Options for _glidix_kopt().
 */
#define	_GLIDIX_KOPT_GFXTERM		0		/* whether the graphics terminal is enabled */

/**
 * A macro to use in the system call table for unused numbers.
 */
#define	SYS_NULL					&sysInvalid

int memcpy_u2k(void *dst_, const void *src_, size_t size)
{
	// user to kernel: src is in userspace
	if (size == 0) return 0;
	uint64_t start = (uint64_t) src_;
	uint64_t end = (uint64_t) src_ + size;
	
	if ((start < ADDR_MIN) || (start >= ADDR_MAX))
	{
		return -1;
	};
	
	if ((end < ADDR_MIN) || (end >= ADDR_MAX))
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
	if (size == 0) return 0;
	uint64_t start = (uint64_t) dst_;
	uint64_t end = (uint64_t) dst_ + size;
	
	if ((start < ADDR_MIN) || (start >= ADDR_MAX))
	{
		return -1;
	};
	
	if ((end < ADDR_MIN) || (end >= ADDR_MAX))
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
		if (addr >= ADDR_MAX)
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
	processExit(WS_EXIT(status));
	kyield();
};

ssize_t sys_write(int fd, const void *buf, size_t size)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	if ((fp->oflags & O_WRONLY) == 0)
	{
		vfsClose(fp);
		ERRNO = EBADF;
		return -1;
	}
	else
	{
		void *tmpbuf = kmalloc(size);
		if (tmpbuf == NULL)
		{
			vfsClose(fp);
			ERRNO = ENOBUFS;
			return -1;
		}
		else if (memcpy_u2k(tmpbuf, buf, size) != 0)
		{
			vfsClose(fp);
			kfree(tmpbuf);
			ERRNO = EFAULT;
			return -1;
		}
		else
		{
			ssize_t out = vfsWrite(fp, tmpbuf, size);
			vfsClose(fp);
			kfree(tmpbuf);
			return out;
		};
	};
};

uint64_t sys_pwrite(int fd, const void *buf, size_t size, off_t offset)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	if ((fp->oflags & O_WRONLY) == 0)
	{
		vfsClose(fp);
		ERRNO = EBADF;
		return -1;
	}
	else
	{
		void *tmpbuf = kmalloc(size);
		if (tmpbuf == NULL)
		{
			vfsClose(fp);
			ERRNO = ENOBUFS;
			return -1;
		}
		else if (memcpy_u2k(tmpbuf, buf, size) != 0)
		{
			vfsClose(fp);
			kfree(tmpbuf);
			ERRNO = EFAULT;
			return -1;
		}
		else
		{
			ssize_t out = vfsPWrite(fp, tmpbuf, size, offset);
			vfsClose(fp);
			kfree(tmpbuf);
			return out;
		};
	};
};

ssize_t sys_read(int fd, void *buf, size_t size)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	if ((fp->oflags & O_RDONLY) == 0)
	{
		vfsClose(fp);
		ERRNO = EBADF;
		return -1;
	}
	else
	{
		void *tmpbuf = kmalloc(size);
		if (tmpbuf == NULL)
		{
			vfsClose(fp);
			ERRNO = ENOBUFS;
			return -1;
		};
		
		ssize_t out = vfsRead(fp, tmpbuf, size);
		size_t toCopy = (size_t) out;
		if (out == -1) toCopy = 0;
		vfsClose(fp);
		if (memcpy_k2u(buf, tmpbuf, toCopy) != 0)
		{
			ERRNO = EFAULT;
			kfree(tmpbuf);
			return -1;
		};
		kfree(tmpbuf);
		
		return out;
	};
};

ssize_t sys_pread(int fd, void *buf, size_t size, off_t offset)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	if ((fp->oflags & O_RDONLY) == 0)
	{
		vfsClose(fp);
		ERRNO = EBADF;
		return -1;
	}
	else
	{
		void *tmpbuf = kmalloc(size);
		if (tmpbuf == NULL)
		{
			vfsClose(fp);
			ERRNO = ENOBUFS;
			return -1;
		};
		
		ssize_t out = vfsPRead(fp, tmpbuf, size, offset);
		size_t toCopy = (size_t) out;
		if (out == -1) toCopy = 0;
		vfsClose(fp);
		if (memcpy_k2u(buf, tmpbuf, toCopy) != 0)
		{
			ERRNO = EFAULT;
			kfree(tmpbuf);
			return -1;
		};
		kfree(tmpbuf);
		
		return out;
	};
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
		ERRNO = EINVAL;
		return -1;
	};
	
	if ((oflag & O_RDWR) == 0)
	{
		/* neither O_RDONLY nor O_WRONLY (and hence not O_RDWR) were set */
		ERRNO = EINVAL;
		return -1;
	};
	
	int fd = ftabAlloc(getCurrentThread()->ftab);
	if (fd == -1)
	{
		ERRNO = EMFILE;
		return -1;
	};
	
	int error;
	File *fp = vfsOpen(VFS_NULL_IREF, path, oflag, mode, &error);
	if (fp == NULL)
	{
		ftabSet(getCurrentThread()->ftab, fd, NULL, 0);
		ERRNO = error;
		return -1;
	};
	
	ftabSet(getCurrentThread()->ftab, fd, fp, oflag & O_CLOEXEC);	/* O_CLOEXEC = FD_CLOEXEC */
	return fd;
};

int sys_close(int fd)
{
	int error = ftabClose(getCurrentThread()->ftab, fd);
	if (error == 0) return 0;
	else
	{
		ERRNO = error;
		return -1;
	};
};

int sys_ioctl(int fd, uint64_t cmd, void *argp)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	if (fp->iref.inode->ioctl == NULL)
	{
		vfsClose(fp);
		ERRNO = ENODEV;
		return -1;
	};

	size_t argsize = (cmd >> 32) & 0xFFFF;
	void *argbuf = kmalloc(argsize);
	if (memcpy_u2k(argbuf, argp, argsize) != 0)
	{
		kfree(argbuf);
		vfsClose(fp);
		ERRNO = EFAULT;
		return -1;
	};
	
	int ret = fp->iref.inode->ioctl(fp->iref.inode, fp, cmd, argbuf);
	memcpy_k2u(argp, argbuf, argsize);		// ignore error
	kfree(argbuf);
	vfsClose(fp);
	return ret;
};

off_t sys_lseek(int fd, off_t offset, int whence)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return (off_t)-1;
	};

	off_t ret = vfsSeek(fp, offset, whence);
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
	memset(&siginfo, 0, sizeof(siginfo_t));
	siginfo.si_signo = sig;
	siginfo.si_code = 0;

	cli();
	lockSched();
	sendSignal(getCurrentThread(), &siginfo);
	unlockSched();
	sti();

	return 0;
};

int sys_stat(const char *upath, struct kstat *buf, size_t bufsz)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (bufsz > sizeof(struct kstat))
	{
		bufsz = sizeof(struct kstat);
	};

	struct kstat kbuf;
	int status = vfsStat(VFS_NULL_IREF, path, 1, &kbuf);
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
		return -1;
	};
};

int sys_lstat(const char *upath, struct kstat *buf, size_t bufsz)
{
	if (bufsz > sizeof(struct kstat))
	{
		bufsz = sizeof(struct kstat);
	};
	
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};

	struct kstat kbuf;
	int status = vfsStat(VFS_NULL_IREF, path, 0, &kbuf);
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
		return -1;
	};
};

int sys_pause()
{
	cli();
	lockSched();
	ERRNO = EINTR;
	waitThread(getCurrentThread());
	unlockSched();
	kyield();
	return -1;
};

int sys_fstat(int fd, struct kstat *buf, size_t bufsz)
{
	struct kstat kbuf;
	
	if (bufsz > sizeof(struct kstat))
	{
		bufsz = sizeof(struct kstat);
	};

	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};

	vfsInodeStat(fp->iref.inode, &kbuf);
	vfsClose(fp);
	
	if (memcpy_k2u(buf, &kbuf, bufsz) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};

	return 0;
};

int sys_chmod(const char *upath, mode_t mode)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return vfsChangeMode(VFS_NULL_IREF, path, mode);
};

int sys_fchmod(int fd, mode_t mode)
{
	// fstat first.
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};

	int ret = vfsInodeChangeMode(fp->iref.inode, mode);
	vfsClose(fp);
	return ret;
};

static int sys_fsync(int fd)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};

	int error = vfsFlush(fp->iref.inode);
	vfsClose(fp);

	if (error != 0)
	{
		ERRNO = error;
		return -1;
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

	return vfsChangeOwner(VFS_NULL_IREF, path, uid, gid);
};

int sys_fchown(int fd, uid_t uid, gid_t gid)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int result = vfsInodeChangeOwner(fp->iref.inode, uid, gid);
	vfsClose(fp);
	return result;
};

int sys_mkdir(const char *upath, mode_t mode)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int error = vfsMakeDir(VFS_NULL_IREF, path, mode);
	if (error != 0)
	{
		ERRNO = error;
		return -1;
	};
	
	return 0;
};

int sys_ftruncate(int fd, off_t length)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};

	if ((fp->oflags & O_WRONLY) == 0)
	{
		vfsClose(fp);
		ERRNO = EBADF;
		return -1;
	};

	int error = vfsTruncate(fp->iref.inode, length);
	vfsClose(fp);
	
	if (error != 0)
	{
		ERRNO = error;
		return -1;
	};
	
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
	
	int error;
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, path, 0, &error);
	if (dref.dent == NULL)
	{
		ERRNO = error;
		return -1;
	};
	
	return 0;
};

int sys_dup(int fd)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int i = ftabAlloc(getCurrentThread()->ftab);
	if (i == -1)
	{
		vfsClose(fp);
		ERRNO = EMFILE;
		return -1;
	};
	
	ftabSet(getCurrentThread()->ftab, i, fp, 0);
	return i;
};

int sys_dup2(int oldfd, int newfd)
{
	if (newfd == oldfd) return newfd;
	
	File *fp = ftabGet(getCurrentThread()->ftab, oldfd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int error = ftabPut(getCurrentThread()->ftab, newfd, fp, 0);
	if (error != 0)
	{
		vfsClose(fp);
		ERRNO = error;
		return -1;
	};
	
	return newfd;
};

int sys_dup3(int oldfd, int newfd, int fdflags)
{
	int allFlags = FD_CLOEXEC;
	if ((fdflags & ~allFlags) != 0)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if (newfd == oldfd) return newfd;
	
	File *fp = ftabGet(getCurrentThread()->ftab, oldfd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int error = ftabPut(getCurrentThread()->ftab, newfd, fp, fdflags);
	if (error != 0)
	{
		vfsClose(fp);
		ERRNO = error;
		return -1;
	};
	
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
	
	if (!havePerm(XP_MODULE))
	{
		ERRNO = EACCES;
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
	
	int error = vfsChangeDir(VFS_NULL_IREF, path);
	if (error != 0)
	{
		ERRNO = error;
		return -1;
	};
	
	return 0;
};

// TODO: update libc to this new signature
size_t sys_getcwd()
{
	char *cwd = vfsGetCurrentDirPath();
	kfree(getCurrentThread()->ktu);
	getCurrentThread()->ktu = cwd;
	getCurrentThread()->ktusz = strlen(cwd)+1;
	return strlen(cwd) + 1;
};

uint64_t sys_mmap(uint64_t addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	if ((prot & ~PROT_ALL) != 0)
	{
		ERRNO = EINVAL;
		return MAP_FAILED;
	};
	
	if ((flags & ~MAP_ALLFLAGS) != 0)
	{
		ERRNO = EINVAL;
		return MAP_FAILED;
	};

	File *fp = NULL;
	if (fd != -1)
	{
		fp = ftabGet(getCurrentThread()->ftab, fd);
		if (fp == NULL)
		{
			ERRNO = EBADF;
			return MAP_FAILED;
		};
	};

	uint64_t result = vmMap(addr, len, prot, flags, fp, offset);
	if (fp != NULL) vfsClose(fp);
	
	if (result < ADDR_MIN)
	{
		ERRNO = (int) result;
		return MAP_FAILED;
	};
	
	return result;
};

int sys_setuid(uid_t uid)
{
	Thread *me = getCurrentThread();
	if ((me->creds->euid != 0) && (uid != me->creds->euid) && (uid != me->creds->ruid))
	{
		ERRNO = EPERM;
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

int sys_setgid(gid_t gid)
{
	Thread *me = getCurrentThread();
	if ((me->creds->egid != 0) && (gid != me->creds->egid) && (gid != me->creds->rgid))
	{
		ERRNO = EPERM;
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

int sys_setreuid(uid_t ruid, uid_t euid)
{
	Thread *me = getCurrentThread();
	uid_t ruidPrev = me->creds->ruid;
	if (ruid != (uid_t)-1)
	{
		if (me->creds->euid != 0)
		{
			ERRNO = EPERM;
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
				ERRNO = EPERM;
				return -1;
			};

			me->creds->suid = euid;
		};

		me->creds->euid = euid;
	};

	return 0;
};

int sys_setregid(gid_t rgid, gid_t egid)
{
	Thread *me = getCurrentThread();
	gid_t rgidPrev = me->creds->rgid;
	if (rgid != (gid_t)-1)
	{
		if (me->creds->egid != 0)
		{
			ERRNO = EPERM;
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
				ERRNO = EPERM;
				return -1;
			};

			me->creds->sgid = egid;
		};

		me->creds->egid = egid;
	};

	return 0;
};

int sys_seteuid(uid_t euid)
{
	Thread *me = getCurrentThread();
	if (me->creds->euid != 0)
	{
		if ((euid != me->creds->euid) && (euid != me->creds->ruid) && (euid != me->creds->suid))
		{
			ERRNO = EPERM;
			return -1;
		};
	};

	me->creds->euid = euid;
	return 0;
};

int sys_setegid(gid_t egid)
{
	Thread *me = getCurrentThread();
	if (me->creds->egid != 0)
	{
		if ((egid != me->creds->egid) && (egid != me->creds->rgid) && (egid != me->creds->sgid))
		{
			ERRNO = EPERM;
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
	
	if (!havePerm(XP_MODULE))
	{
		ERRNO = EPERM;
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
	
	int error = vfsCreateLink(VFS_NULL_IREF, oldname, VFS_NULL_IREF, newname, VFS_AT_SYMLINK_FOLLOW);
	if (error != 0)
	{
		ERRNO = error;
		return -1;
	};
	
	return 0;
};

int sys_symlink(const char *utarget, const char *upath)
{
	char oldname[USER_STRING_MAX];
	if (strcpy_u2k(oldname, utarget) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	char newname[USER_STRING_MAX];
	if (strcpy_u2k(newname, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int error = vfsCreateSymlink(oldname, VFS_NULL_IREF, newname);
	if (error != 0)
	{
		ERRNO = error;
		return -1;
	};
	
	return 0;
};


// TODO: update libc
size_t sys_readlink(const char *upath)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	char *buf = vfsReadLinkPath(VFS_NULL_IREF, path);
	if (buf == NULL)
	{
		return 0;	// error; size=0
	};
	
	// POSIX readlink() doesn't return the terminating NUL, so we can safely ignore that here
	kfree(getCurrentThread()->ktu);
	getCurrentThread()->ktu = buf;
	getCurrentThread()->ktusz = strlen(buf);
	
	return strlen(buf);
};

unsigned sys_sleep(unsigned seconds)
{
	if (seconds == 0) return 0;

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

// TODO: update libc
int sys_utime(const char *upath, time_t atime, uint32_t anano, time_t mtime, uint32_t mnano)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return vfsChangeTimes(VFS_NULL_IREF, path, atime, anano, mtime, mnano);
};

mode_t sys_umask(mode_t cmask)
{
	cmask &= 0777;
	mode_t old = __sync_lock_test_and_set(&getCurrentThread()->creds->umask, cmask);
	return old;
};

int sys_socket(int domain, int type, int proto)
{
	if ((type & ~SOCK_ALLFLAGS) != 0)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	int i = ftabAlloc(getCurrentThread()->ftab);
	if (i == -1)
	{
		ERRNO = EMFILE;
		return -1;
	};
	
	int fdflags = 0;
	if (type & SOCK_CLOEXEC) fdflags |= FD_CLOEXEC;
	
	File *fp = CreateSocket(domain, type & SOCK_TYPEMASK, proto);
	if (fp == NULL)
	{
		ftabSet(getCurrentThread()->ftab, i, NULL, 0);
		// errno set by CreateSocket()
		return -1;
	};
	
	fp->refcount = 1;
	ftabSet(getCurrentThread()->ftab, i, fp, fdflags);
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

	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int out = BindSocket(fp, &addr, addrlen);
	vfsClose(fp);
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

	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		kfree(message);
		return -1;
	};
	
	int out = SendtoSocket(fp, message, len, flags, &addr, addrlen);
	vfsClose(fp);
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
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		out = -1;
	}
	else
	{
		out = RecvfromSocket(fp, message, len, flags, addr, addrlen);
		vfsClose(fp);
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
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		out = -1;
		ERRNO = EBADF;
	}
	else
	{
		out = SocketGetsockname(fp, &addr, &addrlen);
		vfsClose(fp);
	};

	memcpy_k2u(uaddr, &addr, addrlen);
	memcpy_k2u(uaddrlenptr, &addrlen, sizeof(size_t));
	return out;
};

int sys_shutdown(int fd, int how)
{
	if ((how & ~SHUT_RDWR) != 0)
	{
		ERRNO = EINVAL;
		return -1;
	};

	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int out = ShutdownSocket(fp, how);
	vfsClose(fp);
	return out;
};

int sys_listen(int fd, int backlog)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int out = SocketListen(fp, backlog);
	vfsClose(fp);
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

	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int out = ConnectSocket(fp, &kaddr, addrlen);
	vfsClose(fp);
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

	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int out = SocketGetpeername(fp, &addr, &addrlen);
	vfsClose(fp);

	memcpy_k2u(uaddr, &addr, addrlen);
	memcpy_k2u(uaddrlenptr, &addrlen, sizeof(size_t));
	
	return out;
};

int sys_accept4(int fd, struct sockaddr *uaddr, size_t *uaddrlenptr, int flags)
{
	if ((flags & ~SOCK_BITFLAGS) != 0)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	struct sockaddr addr;
	size_t addrlen = INET_SOCKADDR_LEN;
	
	if (uaddrlenptr != NULL)
	{
		if (memcpy_u2k(&addrlen, uaddrlenptr, sizeof(size_t)) != 0)
		{
			ERRNO = EFAULT;
			return -1;
		};
	};
	
	if (addrlen > sizeof(struct sockaddr)) addrlen = sizeof(struct sockaddr);

	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int newfd = ftabAlloc(getCurrentThread()->ftab);
	if (newfd == -1)
	{
		vfsClose(fp);
		ERRNO = EMFILE;
		return -1;
	};
	
	File *newfp = SocketAccept(fp, &addr, &addrlen);
	vfsClose(fp);

	if (newfp == NULL)
	{
		// errno set by SocketAccept()
		ftabSet(getCurrentThread()->ftab, newfd, NULL, 0);
		return -1;
	};
	
	if (uaddr != NULL)
	{
		memcpy_k2u(uaddr, &addr, addrlen);
	};
	
	if (uaddrlenptr != NULL)
	{
		memcpy_k2u(uaddrlenptr, &addrlen, sizeof(size_t));
	};
	
	int fdflags = 0;
	if (flags & SOCK_CLOEXEC) fdflags |= FD_CLOEXEC;
	ftabSet(getCurrentThread()->ftab, newfd, newfp, fdflags);
	
	return newfd;
};

int sys_accept(int fd, struct sockaddr *addr, size_t *addrlenptr)
{
	return sys_accept4(fd, addr, addrlenptr, 0);
};

int sys_setsockopt(int fd, int proto, int option, uint64_t value)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int out = SetSocketOption(fp, proto, option, value);
	vfsClose(fp);
	
	return out;
};

uint64_t sys_getsockopt(int fd, int proto, int option)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	uint64_t out = GetSocketOption(fp, proto, option);
	vfsClose(fp);
	return out;
};

int sys_sockerr(int fd)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int result = SocketGetError(fp);
	vfsClose(fp);
	return result;
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

int sys_fcntl_getfd(int fd)
{
	int flags = ftabGetFlags(getCurrentThread()->ftab, fd);
	if (flags == -1)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	return flags;
};

int sys_fcntl_setfd(int fd, int flags)
{
	if ((flags & ~FD_ALL) != 0)
	{
		ERRNO = EINVAL;
		return -1;
	};

	int error = ftabSetFlags(getCurrentThread()->ftab, fd, flags);
	if (error != 0)
	{
		ERRNO = error;
		return -1;
	};
	
	return 0;
};

int sys_isatty(int fd)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int result = !!(fp->oflags & O_TERMINAL);
	vfsClose(fp);
	return result;
};

int sys_bindif(int fd, const char *uifname)
{
	char ifname[USER_STRING_MAX];
	if (strcpy_u2k(ifname, uifname) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};

	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};

	int out = SocketBindif(fp, ifname);
	vfsClose(fp);
	return out;
};

uint64_t sys_block_on(uint64_t addr, uint64_t expectedVal)
{
	// check alignment
	if (addr & 0x7)
	{
		// not 8-byte-aligned
		return EINVAL;
	};
	
	uint64_t frame = vmGetPhys(addr, PROT_READ);
	if (frame == 0)
	{
		return EFAULT;
	};
	
	uint64_t offset = addr & 0xFFF;
	uint64_t oldFrame = mapTempFrame(frame);
	
	uint64_t physAddr = (frame << 12) | offset;
	uint64_t *ptr = (uint64_t*) tmpframe() + (offset >> 3);
	
	cli();
	lockSched();
	
	if ((*ptr) != expectedVal)
	{
		unlockSched();
		sti();
		mapTempFrame(oldFrame);
		piDecref(frame);
		return 0;
	};
	
	getCurrentThread()->blockPhys = physAddr;
	waitThread(getCurrentThread());
	unlockSched();
	kyield();

	getCurrentThread()->blockPhys = 0;
	mapTempFrame(oldFrame);
	piDecref(frame);
	return 0;
};

uint64_t sys_unblock(uint64_t addr)
{
	// check alignment
	if (addr & 0x7)
	{
		// not 8-byte-aligned
		return EINVAL;
	};
	
	uint64_t frame = vmGetPhys(addr, PROT_READ);
	if (frame == 0)
	{
		return EFAULT;
	};
	
	uint64_t offset = addr & 0xFFF;	
	uint64_t physAddr = (frame << 12) | offset;
	
	cli();
	lockSched();
	
	Thread *ct = getCurrentThread();
	Thread *thread = ct;
	int shouldResched = 0;
	do
	{
		if (thread->blockPhys == physAddr)
		{
			thread->blockPhys = 0;
			shouldResched |= signalThread(thread);
		};
		
		thread = thread->next;
	} while (thread != ct);
	
	unlockSched();
	if (shouldResched) kyield();
	sti();

	piDecref(frame);
	return 0;
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
	regs.fsbase = msrRead(MSR_FS_BASE);
	regs.gsbase = msrRead(MSR_GS_BASE);
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
	if (sec == 0) timeToSignal = 0;
	
	cli();
	lockSched();
	uint64_t old = getCurrentThread()->alarmTime;
	getCurrentThread()->alarmTime = timeToSignal;
	timedCancel(&getCurrentThread()->alarmTimer);
	timedPost(&getCurrentThread()->alarmTimer, timeToSignal);
	unlockSched();
	sti();
	
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

uint64_t sys_fsbase(uint64_t base)
{
	// check if canonical
	uint64_t masked = base & CANON_MASK;
	if ((masked != 0) && (masked != CANON_MASK))
	{
		return EINVAL;
	};
	
	msrWrite(MSR_FS_BASE, base);
	return 0;
};

uint64_t sys_gsbase(uint64_t base)
{
	// check if canonical
	uint64_t masked = base & CANON_MASK;
	if ((masked != 0) && (masked != CANON_MASK))
	{
		return EINVAL;
	};
	
	msrWrite(MSR_GS_BASE, base);
	return 0;
};

int sys_mprotect(uint64_t base, size_t len, int prot)
{
	int status = vmProtect(base, len, prot);
	if (status == 0)
	{
		return 0;
	}
	else
	{
		ERRNO = status;
		return -1;
	};
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
	regs.fsbase = msrRead(MSR_FS_BASE);
	regs.gsbase = msrRead(MSR_GS_BASE);
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

// TODO: update libc
size_t sys_realpath(const char *upath)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return 0;
	};
	
	int error;
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		ERRNO = error;
		return 0;
	};
	
	char *result =  vfsRealPath(dref);
	kfree(getCurrentThread()->ktu);
	getCurrentThread()->ktu = result;
	getCurrentThread()->ktusz = strlen(result) + 1;
	return strlen(result) + 1;
};

void sys_seterrnoptr(int *ptr)
{
	getCurrentThread()->errnoptr = ptr;
};

int* sys_geterrnoptr()
{
	return getCurrentThread()->errnoptr;
};

// TODO: update libc
int sys_unmount(const char *upath, int flags)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int error = vfsUnmount(path, flags);
	if (error != 0)
	{
		ERRNO = error;
		return -1;
	};
	
	return 0;
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
	uint64_t result = vmMap(addr, size, 0, MAP_PRIVATE | MAP_FIXED | MAP_ANON | MAP_UN, NULL, 0);
	if (result < ADDR_MIN)
	{
		ERRNO = (int) result;
		return -1;
	};
	
	return 0;
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
	regs.r12 = getCurrentThread()->sigmask;
	regs.r14 = entry;
	regs.r15 = arg;
	regs.rbp = 0;
	
	if (attr.stack == NULL)
	{
		regs.r13 = attr.stacksize;
	}
	else
	{
		regs.rsp = (uint64_t) attr.stack + attr.stacksize;
		regs.r13 = 0;
	};
	
	// create the thread
	int cloneFlags = CLONE_THREAD;
	if (attr.detachstate == 1)		// detached
	{
		cloneFlags |= CLONE_DETACHED;
	};
	
	uint64_t oldMask = getCurrentThread()->sigmask;
	getCurrentThread()->sigmask = 0xFFFFFFFFFFFFFFFFUL;
	int thid = threadClone(&regs, cloneFlags, NULL);
	getCurrentThread()->sigmask = oldMask;
	
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
		regs.fsbase = msrRead(MSR_FS_BASE);
		regs.gsbase = msrRead(MSR_GS_BASE);
		regs.rdi = 0;
		
		// make sure we retry using the same arguments
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

	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int out = SocketMulticast(fp, op, &addr, scope);
	vfsClose(fp);
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
	int i;
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		if (bitmapReq[i] != 0)
		{
			File *fp = ftabGet(getCurrentThread()->ftab, i);
			if (fp == NULL)
			{
				sems[8*i+PEI_INVALID] = vfsGetConstSem();
			}
			else
			{
				workingFiles[i] = fp;
			};
		};
	};

	// ask all files for their poll information.
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		if (workingFiles[i] != NULL)
		{
			if (workingFiles[i]->iref.inode->pollinfo != NULL)
			{
				workingFiles[i]->iref.inode->pollinfo(workingFiles[i]->iref.inode, workingFiles[i], &sems[8*i]);
				
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

// TODO: update libc to add this system call and also remove fsinfo that it replaced
int sys_fchxperm(int fd, uint64_t ixperm, uint64_t oxperm, uint64_t dxperm)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int result = vfsInodeChangeXPerm(fp->iref.inode, ixperm, oxperm, dxperm);
	if (result == -1)
	{
		int error = ERRNO;
		vfsClose(fp);
		ERRNO = error;
		return -1;
	};
	
	vfsClose(fp);
	return 0;
};

int sys_chxperm(const char *upath, uint64_t ixperm, uint64_t oxperm, uint64_t dxperm)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};

	return vfsChangeXPerm(VFS_NULL_IREF, path, ixperm, oxperm, dxperm);
};

int sys_haveperm(uint64_t mask)
{
	return havePerm(mask);
};

void sys_sync()
{
	sdSync();
};

int sys_nice(int incr)
{
	if (incr < 0)
	{
		if (!havePerm(XP_NICE))
		{
			ERRNO = EPERM;
			return -1;
		};
	};
	
	return thnice(incr);
};

ssize_t sys_procstat(int pid, ProcStat *buf, size_t bufsz)
{
	ProcStat st;
	
	cli();
	lockSched();
	
	Thread *th = getThreadByPID(pid);
	if (th == NULL)
	{
		unlockSched();
		sti();
		
		ERRNO = ESRCH;
		return -1;
	};
	
	if ((th->creds->ruid != getCurrentThread()->creds->ruid) && (getCurrentThread()->creds->euid != 0))
	{
		unlockSched();
		sti();
		
		ERRNO = EPERM;
		return -1;
	};
	
	memcpy(&st, &th->creds->ps, sizeof(ProcStat));
	unlockSched();
	sti();
	
	ssize_t result = sizeof(ProcStat);
	if (bufsz < result)
	{
		result = bufsz;
	};
	
	if (memcpy_k2u(buf, &st, result) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return result;
};

int sys_cpuno()
{
	return getCurrentCPU()->id;
};

int sys_kill(int pid, int sig)
{
	return signalPid(pid, sig);
};

time_t sys_time()
{
	return time();
};

uint64_t sys_nanotime()
{
	return getNanotime();
};

int sys_down(int action)
{
	return systemDown(action);
};

int sys_routetable(uint64_t family)
{
	return sysRouteTable(family);
};

int sys_systat(void *buffer, size_t sz)
{
	SystemState sst;
	memcpy(sst.sst_bootid, bootInfo->bootID, 16);
	
	if (sz > sizeof(SystemState))
	{
		sz = sizeof(SystemState);
	};
	
	if (memcpy_k2u(buffer, &sst, sz) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return 0;
};

int sys_flock_set(int fd, FLock *ulock, int block)
{
	FLock klock;
	if (memcpy_u2k(&klock, ulock, sizeof(FLock)) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (klock.l_whence != VFS_SEEK_CUR && klock.l_whence != VFS_SEEK_SET && klock.l_whence != VFS_SEEK_END)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if (klock.l_type != RL_READ && klock.l_type != RL_WRITE && klock.l_type != RL_UNLOCK)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};

	FileTree *ft = fp->iref.inode->ft;
	if (ft == NULL)
	{
		vfsClose(fp);
		ERRNO = EINVAL;
		return -1;
	};
	
	uint64_t start;
	switch (klock.l_whence)
	{
	case VFS_SEEK_SET:
		start = (uint64_t) klock.l_start;
		break;
	case VFS_SEEK_CUR:
		start = (uint64_t) (fp->offset + klock.l_start);
		break;
	case VFS_SEEK_END:
		start = (uint64_t) (ft->size + klock.l_start);
		break;
	};
	
	uint64_t size;
	if (klock.l_len == 0)
	{
		size = ~start;
	}
	else
	{
		size = (uint64_t) klock.l_len;
	};
	
	int result = ftSetLock(ft, klock.l_type, start, size, block);
	vfsClose(fp);
	
	if (result != 0)
	{
		ERRNO = result;
		return -1;
	};
	
	return 0;
};

int sys_flock_get(int fd, FLock *ulock)
{
	FLock klock;
	if (memcpy_u2k(&klock, ulock, sizeof(FLock)) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (klock.l_whence != VFS_SEEK_SET && klock.l_whence != VFS_SEEK_CUR && klock.l_whence != VFS_SEEK_END)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};

	FileTree *ft = fp->iref.inode->ft;
	if (ft == NULL)
	{
		vfsClose(fp);
		ERRNO = EINVAL;
		return -1;
	};
	
	uint64_t start;
	switch (klock.l_whence)
	{
	case VFS_SEEK_SET:
		start = (uint64_t) klock.l_start;
		break;
	case VFS_SEEK_CUR:
		start = (uint64_t) (fp->offset + klock.l_start);
		break;
	case VFS_SEEK_END:
		start = (uint64_t) (ft->size + klock.l_start);
		break;
	};
	
	uint64_t size;
	int type;
	
	ftGetLock(ft, &type, &klock.l_pid, &start, &size);
	vfsClose(fp);
	
	klock.l_type = type;
	klock.l_whence = VFS_SEEK_SET;
	klock.l_start = start;
	klock.l_len = size;
	
	if (memcpy_k2u(ulock, &klock, sizeof(FLock)) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return 0;
};

int sys_fcntl_setfl(int fd, int oflag)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int allFlags = O_NONBLOCK;
	fp->oflags = (fp->oflags & ~allFlags) | (oflag & allFlags);
	vfsClose(fp);
	return 0;
};

int sys_fcntl_getfl(int fd)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	int oflag = fp->oflags;
	vfsClose(fp);
	return oflag;
};

int sys_aclput(const char *upath, int type, int id, int perms)
{
	if (type != VFS_ACE_USER && type != VFS_ACE_GROUP)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if (id < 0 || ((id & 0xFFFF) != id))
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	int allPerms = 7;
	if ((perms & allPerms) != perms)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};

	int error;
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		ERRNO = error;
		return -1;
	};
	
	InodeRef iref = vfsGetInode(dref, 1, &error);
	if (iref.inode == NULL)
	{
		ERRNO = error;
		return -1;
	};
	
	semWait(&iref.inode->lock);
	if (iref.inode->uid != getCurrentThread()->creds->euid && !havePerm(XP_FSADMIN))
	{
		semSignal(&iref.inode->lock);
		vfsUnrefInode(iref);
		ERRNO = EACCES;
		return -1;
	};
	
	// first try to update the entry if it already exists
	int found = 0;
	int i;
	for (i=0; i<VFS_ACL_SIZE; i++)
	{
		AccessControlEntry *ace = &iref.inode->acl[i];
		if (ace->ace_type == type && ace->ace_id == id)
		{
			ace->ace_perms = perms;
			found = 1;
			break;
		};
	};
	
	if (!found)
	{
		// if not found, then find an empty slot and make a new entry
		for (i=0; i<VFS_ACL_SIZE; i++)
		{
			AccessControlEntry *ace = &iref.inode->acl[i];
			if (ace->ace_type == VFS_ACE_UNUSED)
			{
				ace->ace_id = id;
				ace->ace_type = type;
				found = 1;
				break;
			};
		};
	};
	
	if (!found)
	{
		// if still not found, then the ACL is overflowed
		semSignal(&iref.inode->lock);
		vfsUnrefInode(iref);
		ERRNO = EOVERFLOW;
		return -1;
	};
	
	vfsDirtyInode(iref.inode);
	semSignal(&iref.inode->lock);
	vfsUnrefInode(iref);
	return 0;
};

int sys_aclclear(const char *upath, int type, int id)
{
	if (type != VFS_ACE_USER && type != VFS_ACE_GROUP)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if (id < 0 || ((id & 0xFFFF) != id))
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};

	int error;
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, path, 0, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		ERRNO = error;
		return -1;
	};
	
	InodeRef iref = vfsGetInode(dref, 1, &error);
	if (iref.inode == NULL)
	{
		ERRNO = error;
		return -1;
	};
	
	semWait(&iref.inode->lock);
	if (iref.inode->uid != getCurrentThread()->creds->euid && !havePerm(XP_FSADMIN))
	{
		semSignal(&iref.inode->lock);
		vfsUnrefInode(iref);
		ERRNO = EACCES;
		return -1;
	};
	
	int i;
	for (i=0; i<VFS_ACL_SIZE; i++)
	{
		AccessControlEntry *ace = &iref.inode->acl[i];
		if (ace->ace_type == type && ace->ace_id == id)
		{
			ace->ace_id = ace->ace_type = ace->ace_perms = 0;
			break;
		};
	};
	
	vfsDirtyInode(iref.inode);
	semSignal(&iref.inode->lock);
	vfsUnrefInode(iref);
	return 0;
};

size_t sys_getdent(int fd, int key)
{
	File *fp = ftabGet(getCurrentThread()->ftab, fd);
	if (fp == NULL)
	{
		ERRNO = EBADF;
		return -1;
	};
	
	struct kdirent *dirent;
	ssize_t size = vfsReadDir(fp->iref.inode, key, &dirent);
	if (size < 0)
	{
		vfsClose(fp);
		ERRNO = -size;
		return 0;
	};
	
	vfsClose(fp);
	
	kfree(getCurrentThread()->ktu);
	getCurrentThread()->ktu = dirent;
	getCurrentThread()->ktusz = size;
	return size;
};

int sys_getktu(void *buffer, size_t size)
{
	if (getCurrentThread()->ktusz != size)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	if (memcpy_k2u(buffer, getCurrentThread()->ktu, size) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	kfree(getCurrentThread()->ktu);
	getCurrentThread()->ktu = NULL;
	getCurrentThread()->ktusz = 0;
	return 0;
};

/**
 * System call table for fast syscalls, and the number of system calls.
 * Do not use NULL entries! Instead, for unused entries, enter SYS_NULL.
 */
#define SYSCALL_NUMBER 145
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
	&sys_fsbase,				// 19
	&sys_gsbase,				// 20
	&sys_mprotect,				// 21
	&sys_lseek,				// 22
	&sys_fork,				// 23
	&sys_pause,				// 24
	&sys_waitpid,				// 25
	&sys_kill,				// 26
	&sys_insmod,				// 27
	&sys_ioctl,				// 28
	&sys_getdent,				// 29
	SYS_NULL,				// 30 (_glidix_diag())
	&sys_mount,				// 31
	&sys_yield,				// 32
	&sys_time,				// 33
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
	&sys_nanotime,				// 51
	&sys_pread,				// 52
	&sys_pwrite,				// 53
	&sys_mmap,				// 54
	&sys_setuid,				// 55
	&sys_setgid,				// 56
	&sys_seteuid,				// 57
	&sys_setegid,				// 58
	&sys_setreuid,				// 59
	&sys_setregid,				// 60
	&sys_rmmod,				// 61
	&sys_link,				// 62
	&sys_unmount,				// 63
	&sys_lstat,				// 64
	&sys_symlink,				// 65
	&sys_readlink,				// 66
	&sys_down,				// 67
	&sys_sleep,				// 68
	&sys_utime,				// 69
	&sys_umask,				// 70
	&sys_routetable,			// 71
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
	&sys_fcntl_getfd,			// 93
	&sys_fcntl_setfd,			// 94
	&sys_unique,				// 95
	&sys_isatty,				// 96
	&sys_bindif,				// 97
	&sys_route_clear,			// 98
	&sys_munmap,				// 99
	&sys_pipe2,				// 100
	&sys_getppid,				// 101
	&sys_alarm,				// 102
	&sys_block_on,				// 103
	&sys_dup3,				// 104
	&sys_unblock,				// 105
	&sys_trace,				// 106
	&sys_listen,				// 107
	&sys_accept,				// 108
	&sys_accept4,				// 109
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
	&sys_sockerr,				// 123
	&sys_mcast,				// 124
	&sys_fpoll,				// 125
	&sys_oxperm,				// 126
	&sys_dxperm,				// 127
	&sys_fchxperm,				// 128
	&sys_chxperm,				// 129
	&sys_haveperm,				// 130
	&sys_sync,				// 131
	&sys_nice,				// 132
	&sys_procstat,				// 133
	&sys_cpuno,				// 134
	&sysInvalid,				// 135 [guaranteed to always be unused!]
	&sys_systat,				// 136
	&sys_fsdrv,				// 137
	&sys_flock_set,				// 138
	&sys_flock_get,				// 139
	&sys_fcntl_setfl,			// 140
	&sys_fcntl_getfl,			// 141
	&sys_aclput,				// 142
	&sys_aclclear,				// 143
	&sys_getktu,				// 144
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
				*(getCurrentThread()->errnoptr) = ERRNO;
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
		regs.fsbase = msrRead(MSR_FS_BASE);
		regs.gsbase = msrRead(MSR_GS_BASE);
		cli();
		switchTask(&regs);
	};

	return retval;
};
