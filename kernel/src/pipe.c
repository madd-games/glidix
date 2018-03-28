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

#include <glidix/pipe.h>
#include <glidix/memory.h>
#include <glidix/sched.h>
#include <glidix/string.h>
#include <glidix/syscall.h>

static void* pipe_open(Inode *inode, int oflags)
{
	Pipe *pipe = (Pipe*) inode->fsdata;
	
	if ((oflags & O_RDWR) == O_RDWR)
	{
		// cannot open in read/write mode
		ERRNO = EINVAL;
		return NULL;
	};
	
	if (oflags & O_RDONLY)
	{
		if (oflags & O_NONBLOCK)
		{
			semSignal(&pipe->semReadOpen);
			return (void*) 1;
		}
		else
		{
			semSignal(&pipe->semReadOpen);
			
			uint8_t bitmap = 0;
			Semaphore *sem = &pipe->semWriteOpen;
			int error = semPoll(1, &sem, &bitmap, SEM_W_INTR, 0);
			
			if (error < 0)
			{
				ERRNO = error;
				return NULL;
			};
			
			__sync_fetch_and_add(&pipe->cntRead, 1);
			return PIPE_READ;
		};
	}
	else
	{
		// opening for write
		semSignal(&pipe->semWriteOpen);
		
		uint8_t bitmap = 0;
		Semaphore *sem = &pipe->semReadOpen;
		int error = semPoll(1, &sem, &bitmap, SEM_W_FILE(oflags), 0);
		
		if (error < 0)
		{
			ERRNO = -error;
			return NULL;
		};
		
		if (error == 0)
		{
			ERRNO = ENXIO;
			return NULL;
		};
		
		__sync_fetch_and_add(&pipe->cntWrite, 1);
		return PIPE_WRITE;
	};
};

static void pipe_close(Inode *inode, void *filedata)
{
	Pipe *pipe = (Pipe*) inode->fsdata;
	
	if (filedata == PIPE_READ)
	{
		if (__sync_add_and_fetch(&pipe->cntRead, -1) == 0)
		{
			semTerminate(&pipe->semBufferFree);
		};
	}
	else
	{
		if (__sync_add_and_fetch(&pipe->cntWrite, -1) == 0)
		{
			semTerminate(&pipe->semBufferRead);
			semSignal(&pipe->semHangup);
		};
	};
};

static ssize_t pipe_read(Inode *inode, File *fp, void *buffer, size_t size, off_t off)
{
	Pipe *pipe = (Pipe*) inode->fsdata;
	
	int count = semWaitGen(&pipe->semBufferRead, (int) size, SEM_W_FILE(fp->oflags), 0);
	if (count < 0)
	{
		ERRNO = -count;
		return -1;
	};
	
	char *put = (char*) buffer;
	ssize_t out = 0;
	
	semWait(&pipe->lock);
	while (count--)
	{
		*put++ = pipe->buffer[pipe->offRead];
		pipe->offRead = (pipe->offRead + 1) % PIPE_BUFFER_SIZE;
		out++;
	};
	semSignal(&pipe->lock);
	
	return out;
};

static ssize_t pipe_write(Inode *inode, File *fp, const void *buffer, size_t size, off_t off)
{
	Pipe *pipe = (Pipe*) inode->fsdata;
	
	ssize_t out = 0;
	const char *scan = (const char*) buffer;
	
	while (size > 0)
	{
		int count = semWaitGen(&pipe->semBufferFree, (int) size, SEM_W_FILE(fp->oflags), 0);
		if (count < 0)
		{
			ERRNO = -count;
			if (out == 0) return -1;
			else return out;
		};
		
		if (pipe->cntRead == 0 || count == 0)
		{
			// no read end is open
			siginfo_t siginfo;
			memset(&siginfo, 0, sizeof(siginfo_t));
			
			siginfo.si_signo = SIGPIPE;
			siginfo.si_code = 0;

			cli();
			lockSched();
			sendSignal(getCurrentThread(), &siginfo);
			unlockSched();
			sti();
		
			ERRNO = EPIPE;
			return -1;
		};
		
		semWait(&pipe->lock);
		while (count--)
		{
			pipe->buffer[pipe->offWrite] = *scan++;
			pipe->offWrite = (pipe->offWrite + 1) % PIPE_BUFFER_SIZE;
			out++;
			size--;
		};
		semSignal(&pipe->lock);
	};
	
	return out;
};

static void pipe_pollinfo(Inode *inode, File *fp, Semaphore **sems)
{
	Pipe *pipe = (Pipe*) inode->fsdata;
	sems[PEI_READ] = &pipe->semBufferRead;
	sems[PEI_WRITE] = &pipe->semBufferFree;
	sems[PEI_HANGUP] = &pipe->semHangup;
};

static Inode* pipeCreate(mode_t mode)
{
	Inode *inode = vfsCreateInode(NULL, (mode & 0xFFF) | VFS_MODE_FIFO);
	Pipe *pipe = NEW(Pipe);
	
	semInit2(&pipe->semReadOpen, 0);
	semInit2(&pipe->semWriteOpen, 0);
	semInit2(&pipe->semBufferRead, 0);
	semInit2(&pipe->semBufferFree, PIPE_BUFFER_SIZE);

	pipe->offRead = 0;
	pipe->offWrite = 0;
	
	pipe->cntRead = 0;
	pipe->cntWrite = 0;

	semInit2(&pipe->semHangup, 0);	
	semInit(&pipe->lock);
	
	inode->fsdata = pipe;
	inode->pread = pipe_read;
	inode->pwrite = pipe_write;
	inode->open = pipe_open;
	inode->close = pipe_close;
	inode->pollinfo = pipe_pollinfo;
	
	return inode;
};

int sys_pipe(int *upipefd)
{
	return sys_pipe2(upipefd, 0);
};

int sys_pipe2(int *upipefd, int flags)
{
	int allFlags = O_CLOEXEC | O_NONBLOCK;
	if ((flags & ~allFlags) != 0)
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	int pipefd[2];

	int rfd = ftabAlloc(getCurrentThread()->ftab);
	if (rfd == -1)
	{
		ERRNO = EMFILE;
		return -1;
	};
	
	int wfd = ftabAlloc(getCurrentThread()->ftab);
	if (wfd == -1)
	{
		ftabSet(getCurrentThread()->ftab, rfd, NULL, 0);
		ERRNO = EMFILE;
		return -1;
	};
	
	Inode *inode = pipeCreate(0600);
	Pipe *pipe = (Pipe*) inode->fsdata;
	semSignal(&pipe->semReadOpen);
	semSignal(&pipe->semWriteOpen);
	
	InodeRef iref;
	iref.inode = inode;
	iref.top = NULL;
	
	int error;
	File *rfp = vfsOpenInode(iref, O_RDONLY | flags, &error);
	File *wfp = vfsOpenInode(iref, O_WRONLY | flags, &error);
	assert(rfp != NULL);
	assert(wfp != NULL);
	
	inode->refcount = 2;	// both the open descriptors (vfsOpenInode does not update the refcount)

	// O_CLOEXEC == FD_CLOEXEC
	ftabSet(getCurrentThread()->ftab, rfd, rfp, flags & O_CLOEXEC);
	ftabSet(getCurrentThread()->ftab, wfd, wfp, flags & O_CLOEXEC);
	
	pipefd[0] = rfd;
	pipefd[1] = wfd;

	if (memcpy_k2u(upipefd, pipefd, sizeof(int)*2) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return 0;
};

int sys_mkfifo(const char *upath, mode_t mode)
{
	char path[USER_STRING_MAX];
	if (strcpy_u2k(path, upath) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	int error;
	DentryRef dref = vfsGetDentry(VFS_NULL_IREF, path, 1, &error);
	if (dref.dent == NULL)
	{
		vfsUnrefDentry(dref);
		ERRNO = error;
		return -1;
	};
	
	if (dref.dent->ino != 0)
	{
		vfsUnrefDentry(dref);
		ERRNO = EEXIST;
		return -1;
	};
	
	Inode *pipe = pipeCreate(mode);
	vfsBindInode(dref, pipe);
	vfsDownrefInode(pipe);
	
	return 0;
};
