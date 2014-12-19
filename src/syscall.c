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

static uint64_t sys_write(int fd, const void *buf, size_t size)
{
	ssize_t out;
	if (fd >= MAX_OPEN_FILES)
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
				getCurrentThread()->therrno = EACCES;
				out = -1;
			}
			else
			{
				out = fp->write(fp, buf, size);
				spinlockRelease(&ftab->spinlock);
			};
		};
	};

	return *((uint64_t*)&out);
};

static uint64_t sys_read(int fd, void *buf, size_t size)
{
	ssize_t out;
	if (fd >= MAX_OPEN_FILES)
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
				getCurrentThread()->therrno = EACCES;
				out = -1;
			}
			else
			{
				out = fp->read(fp, buf, size);
				spinlockRelease(&ftab->spinlock);
			};
		};
	};

	return *((uint64_t*)&out);
};

static int sysOpenErrno(int vfsError)
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
	default:
		/* fallback in case there are some unhandled errors */
		getCurrentThread()->therrno = EIO;
		break;
	};
	return -1;
};

static int sys_open(const char *path, int oflag, mode_t mode)
{
	if ((oflag & O_ALL) != oflag)
	{
		/* unrecognised bits were set */
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};

	struct stat st;
	int error = vfsStat(path, &st);
	if (error != 0)
	{
		return sysOpenErrno(error);
	};

	int neededPerms = 0;
	if (oflag & O_WRONLY)
	{
		neededPerms |= 2;
	};
	if (oflag & O_WRONLY)
	{
		neededPerms |= 4;
	};

	if (!vfsCanCurrentThread(&st, neededPerms))
	{
		return sysOpenErrno(VFS_PERM);
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
		return sysOpenErrno(VFS_FILE_LIMIT_EXCEEDED);
	};

	File *fp = vfsOpen(path, VFS_CHECK_ACCESS, &error);
	if (fp == NULL)
	{
		spinlockRelease(&ftab->spinlock);
		return sysOpenErrno(error);
	};

	if (oflag & O_APPEND)
	{
		if (fp->seek != NULL) fp->seek(fp, 0, SEEK_END);
	};

	ftab->entries[i] = fp;
	spinlockRelease(&ftab->spinlock);

	return i;
};

static void sys_close(int fd)
{
	if (fd >= MAX_OPEN_FILES)
	{
		return;
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
};

static off_t sys_lseek(int fd, off_t offset, int whence)
{
	if (fd >= MAX_OPEN_FILES)
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

static int sys_raise(Regs *regs, int sig)
{
	if ((sig < 0) || (sig >= SIG_NUM))
	{
		return -1;
	};

	siginfo_t siginfo;
	siginfo.si_signo = sig;
	siginfo.si_code = 0;

	ASM("cli");
	sendSignal(getCurrentThread(), &siginfo);
	regs->rax = 0;
	switchTask(regs);

	// not actually stored in RAX because of the task switch, but libglidix should set
	// RAX to zero before executing UD2.
	return 0;
};

static int sys_stat(const char *path, struct stat *buf)
{
	int status = vfsStat(path, buf);
	if (status == 0)
	{
		return status;
	}
	else
	{
		getCurrentThread()->therrno = EIO;
		return -1;
	};
};

static int isPointerValid(uint64_t ptr, uint64_t size)
{
	if (ptr < 0x1000)
	{
		return 0;
	};

	if ((ptr+size) > 0x8000000000)
	{
		return 0;
	};

	return 1;
};

static void signalOnBadPointer(Regs *regs, uint64_t ptr)
{
	siginfo_t siginfo;
	siginfo.si_signo = SIGSEGV;
	siginfo.si_code = SEGV_ACCERR;
	siginfo.si_addr = (void*) ptr;

	ASM("cli");
	sendSignal(getCurrentThread(), &siginfo);
	switchTask(regs);
};

static void signalOnBadSyscall(Regs *regs)
{
	siginfo_t si;
	si.si_signo = SIGSYS;

	ASM("cli");
	sendSignal(getCurrentThread(), &si);
	switchTask(regs);
};

void syscallDispatch(Regs *regs, uint16_t num)
{
	regs->rip += 4;

	switch (num)
	{
	case 1:
		if (!isPointerValid(regs->rsi, regs->rdx))
		{
			signalOnBadPointer(regs, regs->rsi);
		};
		*((ssize_t*)&regs->rax) = sys_write(*((int*)&regs->rdi), (const void*) regs->rsi, *((size_t*)&regs->rdx));
		break;
	case 2:
		if (!isPointerValid(regs->rsi, regs->rdx))
		{
			signalOnBadPointer(regs, regs->rsi);
		};
		regs->rax = elfExec(regs, (const char*) regs->rdi, (const char*) regs->rsi, regs->rdx);
		break;
	case 3:
		if (!isPointerValid(regs->rsi, regs->rdx))
		{
			signalOnBadPointer(regs, regs->rsi);
		};
		*((ssize_t*)&regs->rax) = sys_read(*((int*)&regs->rdi), (void*) regs->rsi, *((size_t*)&regs->rdx));
		break;
	case 4:
		regs->rax = sys_open((const char*) regs->rdi, (int) regs->rsi, (mode_t) regs->rdx);
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
		if (!isPointerValid(regs->rdi, 0))
		{
			signalOnBadPointer(regs, regs->rdi);
		};
		getCurrentThread()->rootSigHandler = regs->rdi;
		regs->rax = 0;
		break;
	case 14:
		if (!isPointerValid(regs->rdi, 0x1000))
		{
			signalOnBadPointer(regs, regs->rdi);
		};
		sigret(regs, (void*) regs->rdi);
		break;
	case 15:
		if (!isPointerValid(regs->rsi, sizeof(struct stat)))
		{
			signalOnBadPointer(regs, regs->rsi);
		};
		*((int*)&regs->rax) = sys_stat((const char*) regs->rdi, (struct stat*) regs->rsi);
		break;
	case 16:
		regs->rax = (uint64_t) getCurrentThread()->szExecPars;
		break;
	case 17:
		if (!isPointerValid(regs->rdi, regs->rsi))
		{
			signalOnBadPointer(regs, regs->rdi);
		};
		memcpy((void*)regs->rdi, getCurrentThread()->execPars, regs->rsi);
		break;
	case 18:
		*((int*)&regs->rax) = sys_raise(regs, (int) regs->rdi);
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
	default:
		signalOnBadSyscall(regs);
		break;
	};
};
