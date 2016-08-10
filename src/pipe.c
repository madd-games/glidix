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

#include <glidix/pipe.h>
#include <glidix/sched.h>
#include <glidix/memory.h>
#include <glidix/errno.h>
#include <glidix/string.h>
#include <glidix/syscall.h>
#include <glidix/signal.h>
#include <glidix/console.h>

static File *openPipe(Pipe *pipe, int mode);

static ssize_t pipe_read(File *fp, void *buffer, size_t size)
{
	//kprintf_debug("pipe_read called (size=%d)\n", (int) size);
	if (size == 0) return 0;

	Pipe *pipe = (Pipe*) fp->fsdata;
	
	ssize_t sizeCanRead = 0;
	if (fp->oflag & O_NONBLOCK)
	{
		int count = semWaitNoblock(&pipe->counter, (int) size);
		if (count == -1)
		{
			ERRNO = EAGAIN;
			return -1;
		};
		
		sizeCanRead = (ssize_t) count;
	}
	else
	{
		int count = semWaitTimeout(&pipe->counter, (int) size, 0);
		if (count < 0)
		{
			ERRNO = EINTR;
			return -1;
		};
		
		sizeCanRead = (ssize_t) count;
	};
	
	if (sizeCanRead == 0)
	{
		// EOF
		return sizeCanRead;
	};
	
	semWait(&pipe->sem);

	char *put = (char*) buffer;
	ssize_t bytesLeft = (ssize_t)size;
	if (bytesLeft > sizeCanRead) bytesLeft = sizeCanRead;
	while (bytesLeft--)
	{
		*put++ = pipe->buffer[pipe->roff];
		pipe->roff = (pipe->roff + 1) % PIPE_BUFFER_SIZE;
	};
	
	semSignal(&pipe->sem);
	return sizeCanRead;
};

static ssize_t pipe_write(File *fp, const void *buffer, size_t size)
{
	//kprintf_debug("pipe_write called (size=%d)\n", (int) size);

	Pipe *pipe = (Pipe*) fp->fsdata;
	semWait(&pipe->sem);

	if ((pipe->sides & SIDE_READ) == 0)
	{
		// the pipe is not readable!
		siginfo_t siginfo;
		siginfo.si_signo = SIGPIPE;
		siginfo.si_code = 0;

		cli();
		lockSched();
		sendSignal(getCurrentThread(), &siginfo);
		unlockSched();
		sti();
		
		ERRNO = EPIPE;
		semSignal(&pipe->sem);
		return -1;
	};
	
	// determine the maximum amount of data we can currently write.
	ssize_t willWrite = (pipe->roff + PIPE_BUFFER_SIZE) - pipe->woff;
	if (willWrite > (ssize_t)size) willWrite = (ssize_t) size;
	
	// write it
	const char *scan = (const char*) buffer;
	ssize_t bytesLeft = willWrite;
	while (bytesLeft--)
	{
		pipe->buffer[pipe->woff] = *scan++;
		pipe->woff = (pipe->woff + 1) % PIPE_BUFFER_SIZE;
	};
	
	// wake up waiting threads
	semSignal2(&pipe->counter, (int) willWrite);
	
	semSignal(&pipe->sem);
	return willWrite;
};

static void pipe_close(File *fp)
{
	Pipe *pipe = (Pipe*) fp->fsdata;
	semWait(&pipe->sem);

	if (fp->oflag & O_WRONLY)
	{
		pipe->sides &= ~SIDE_WRITE;
		semTerminate(&pipe->counter);
	}
	else
	{
		pipe->sides &= ~SIDE_READ;
	};
	
	if (pipe->sides == 0)
	{
		kfree(pipe);
	}
	else
	{
		semSignal(&pipe->sem);
	};
};

static int pipe_fstat(File *fp, struct stat *st)
{
	Pipe *pipe = (Pipe*) fp->fsdata;
	semWait(&pipe->sem);

	st->st_dev = 0;
	st->st_ino = (ino_t) fp->fsdata;
	st->st_mode = 0600 | VFS_MODE_FIFO;
	st->st_nlink = ((pipe->sides >> 1) & 1) + (pipe->sides & 1);
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = 0;
	st->st_size = PIPE_BUFFER_SIZE;
	st->st_blksize = 1;
	st->st_blocks = PIPE_BUFFER_SIZE;
	st->st_atime = 0;
	st->st_mtime = 0;
	st->st_ctime = 0;

	semSignal(&pipe->sem);
	return 0;
};

static File *openPipe(Pipe *pipe, int mode)
{
	semWait(&pipe->sem);
	File *fp = (File*) kmalloc(sizeof(File));
	memset(fp, 0, sizeof(File));

	fp->oflag = mode;
	fp->fsdata = pipe;
	fp->oflag = mode;
	if (mode == O_RDONLY)
	{
		fp->read = pipe_read;
	}
	else
	{
		fp->write = pipe_write;
	};
	fp->close = pipe_close;
	fp->fstat = pipe_fstat;
	fp->refcount = 1;

	semSignal(&pipe->sem);
	return fp;
};

int sys_pipe(int *upipefd)
{
	int pipefd[2];

	int rfd=-1, wfd=-1;

	FileTable *ftab = getCurrentThread()->ftab;
	spinlockAcquire(&ftab->spinlock);

	int i;
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		if (ftab->entries[i] == NULL)
		{
			if (rfd == -1)
			{
				rfd = i;
			}
			else if (wfd == -1)
			{
				wfd = i;
				break;
			};
		};
	};

	if ((rfd == -1) || (wfd == -1))
	{
		spinlockRelease(&ftab->spinlock);
		getCurrentThread()->therrno = EMFILE;
		return -1;
	};

	Pipe *pipe = (Pipe*) kmalloc(sizeof(Pipe));
	semInit(&pipe->sem);
	semInit2(&pipe->counter, 0);
	pipe->roff = 0;
	pipe->woff = 0;
	pipe->sides = SIDE_READ | SIDE_WRITE;

	ftab->entries[rfd] = openPipe(pipe, O_RDONLY);
	ftab->entries[wfd] = openPipe(pipe, O_WRONLY);

	pipefd[0] = rfd;
	pipefd[1] = wfd;

	spinlockRelease(&ftab->spinlock);
	
	if (memcpy_k2u(upipefd, pipefd, sizeof(int)*2) != 0)
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	return 0;
};
