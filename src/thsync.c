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

#include <glidix/thsync.h>
#include <glidix/errno.h>
#include <glidix/sched.h>
#include <glidix/mutex.h>
#include <glidix/semaphore.h>
#include <glidix/memory.h>
#include <glidix/string.h>

static void thsync_close(File *fp)
{
	kfree(fp->fsdata);
};

static int thsync_mutex_ioctl(File *fp, uint64_t cmd, void *argp)
{
	switch (cmd)
	{
	case IOCTL_MUTEX_LOCK:
		mutexLock((Mutex*)fp->fsdata);
		return 0;
	case IOCTL_MUTEX_UNLOCK:
		mutexUnlock((Mutex*)fp->fsdata);
		return 0;
	};
	
	ERRNO = EINVAL;
	return -1;
};

static int thsync_sema_ioctl(File *fp, uint64_t cmd, void *argp)
{
	int result;
	switch (cmd)
	{
	case IOCTL_SEMA_WAIT:
		result = semWaitGen((Semaphore*)fp->fsdata, *((int*)argp), SEM_W_INTR, 0);
		if (result > 0)
		{
			return result;
		};
		ERRNO = EINTR;
		return -1;
	case IOCTL_SEMA_SIGNAL:
		semSignal2((Semaphore*)fp->fsdata, *((int*)argp));
		return 0;
	};
	
	ERRNO = EINVAL;
	return -1;
};

File* thsync(int type, int par)
{
	if ((type != THSYNC_MUTEX) && (type != THSYNC_SEMA))
	{
		ERRNO = EINVAL;
		return NULL;
	};
	
	File *fp = NEW(File);
	memset(fp, 0, sizeof(File));
	
	if (type == THSYNC_MUTEX)
	{
		Mutex *mutex = NEW(Mutex);
		mutexInit(mutex);
		fp->fsdata = mutex;
		fp->ioctl = thsync_mutex_ioctl;
	}
	else
	{
		Semaphore *sem = NEW(Semaphore);
		semInit2(sem, par);
		fp->fsdata = sem;
		fp->ioctl = thsync_sema_ioctl;
	};
	
	fp->oflag = O_CLOEXEC;
	fp->close = thsync_close;
	fp->refcount = 1;
	return fp;
};
