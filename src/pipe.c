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

static File *openPipe(Pipe *pipe, int mode);

static ssize_t pipe_read(File *fp, void *buffer, size_t size)
{
	if (size == 0) return 0;

	Pipe *pipe = (Pipe*) fp->fsdata;
	semWait(&pipe->sem);

	if (pipe->size == 0)
	{
		if (pipe->writecount == 0)
		{
			semSignal(&pipe->sem);
			return 0;
		}
		else if (fp->oflag & O_NONBLOCK)
		{
			semSignal(&pipe->sem);
			getCurrentThread()->therrno = EAGAIN;
			return -1;
		}
		else
		{
			semSignal(&pipe->sem);
			while (1)
			{
				if (getCurrentThread()->sigcnt > 0)
				{
					getCurrentThread()->therrno = EINTR;
					return -1;
				};

				semWait(&pipe->sem);
				if (pipe->size > 0) break;
				semSignal(&pipe->sem);
			};
		};
	};

	if (size > pipe->size) size = pipe->size;

	ssize_t outSize = 0;
	uint8_t *out = (uint8_t*) buffer;
	while (size--)
	{
		*out++ = pipe->buffer[pipe->offRead];
		pipe->offRead = (pipe->offRead + 1) % 1024;
		outSize++;
		pipe->size--;
	};

	semSignal(&pipe->sem);
	return outSize;
};

static ssize_t pipe_write(File *fp, const void *buffer, size_t size)
{
	if (size == 0) return 0;

	Pipe *pipe = (Pipe*) fp->fsdata;
	semWait(&pipe->sem);

	if (pipe->readcount == 0)
	{
		semSignal(&pipe->sem);
		return 0;
	};

	if ((pipe->size+size) > 1024)
	{
		if (fp->oflag & O_NONBLOCK)
		{
			semSignal(&pipe->sem);
			getCurrentThread()->therrno = EAGAIN;
			return -1;
		}
		else
		{
			semSignal(&pipe->sem);
			while (1)
			{
				if (getCurrentThread()->sigcnt > 0)
				{
					getCurrentThread()->therrno = EINTR;
					return -1;
				};
				semWait(&pipe->sem);
				if ((pipe->size+size) <= 1024) break;
				semSignal(&pipe->sem);
			};
		};
	};

	ssize_t inSize = 0;
	const uint8_t *in = (const uint8_t*) buffer;
	while (size--)
	{
		pipe->buffer[pipe->offWrite] = *in++;;
		pipe->offWrite = (pipe->offWrite + 1) % 1024;
		inSize++;
		pipe->size++;
	};

	semSignal(&pipe->sem);
	return inSize;
};

static void pipe_close(File *fp)
{
	Pipe *pipe = (Pipe*) fp->fsdata;
	semWait(&pipe->sem);

	if (fp->oflag & O_WRONLY)
	{
		pipe->writecount--;
	}
	else
	{
		pipe->readcount--;
	};

	if ((pipe->readcount == 0) && (pipe->writecount == 0))
	{
		kfree(pipe);
	}
	else
	{
		semSignal(&pipe->sem);
	};
};

static int pipe_dup(File *fp, File *newfp, size_t szfile)
{
	File *x = openPipe((Pipe*)fp->fsdata, fp->oflag & O_RDWR);
	memcpy(newfp, x, sizeof(File));
	kfree(x);
	return 0;
};

static int pipe_fstat(File *fp, struct stat *st)
{
	Pipe *pipe = (Pipe*) fp->fsdata;
	semWait(&pipe->sem);

	st->st_dev = 0;
	st->st_ino = (ino_t) fp->fsdata;
	st->st_mode = 0600 | VFS_MODE_FIFO;
	st->st_nlink = pipe->readcount + pipe->writecount;
	st->st_uid = 0;
	st->st_gid = 0;
	st->st_rdev = 0;
	st->st_size = pipe->size;
	st->st_blksize = 1;
	st->st_blocks = pipe->size;
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

	if (mode == O_RDONLY)
	{
		pipe->readcount++;
	}
	else
	{
		pipe->writecount++;
	};

	fp->fsdata = pipe;
	fp->oflag = mode;
	fp->read = pipe_read;
	fp->write = pipe_write;
	fp->close = pipe_close;
	fp->dup = pipe_dup;
	fp->fstat = pipe_fstat;

	semSignal(&pipe->sem);
	return fp;
};

int sys_pipe(int *pipefd)
{
	if (!isPointerValid((uint64_t)pipefd, sizeof(int)*2, PROT_WRITE))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
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
	pipe->readcount = 0;
	pipe->writecount = 0;
	pipe->offRead = 0;
	pipe->offWrite = 0;
	pipe->size = 0;

	ftab->entries[rfd] = openPipe(pipe, O_RDONLY);
	ftab->entries[wfd] = openPipe(pipe, O_WRONLY);

	pipefd[0] = rfd;
	pipefd[1] = wfd;

	spinlockRelease(&ftab->spinlock);
	return 0;
};
