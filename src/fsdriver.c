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

#include <glidix/fsdriver.h>
#include <glidix/semaphore.h>
#include <glidix/sched.h>
#include <glidix/memory.h>
#include <glidix/mount.h>
#include <glidix/string.h>
#include <glidix/syscall.h>

static Semaphore semFS;
static FSDriver *firstDriver;

void initFSDrivers()
{
	firstDriver = NULL;
	semInit(&semFS);
};

void registerFSDriver(FSDriver *drv)
{
	semWait(&semFS);
	if (firstDriver == NULL)
	{
		firstDriver = drv;
		drv->prev = NULL;
		drv->next = NULL;
	}
	else
	{
		FSDriver *last = firstDriver;
		while (last->next != NULL) last = last->next;
		last->next = drv;
		drv->prev = last;
		drv->next = NULL;
	};
	semSignal(&semFS);
};

int sys_mount(const char *fsname, const char *image, const char *mountpoint, int flags)
{
	if (!isStringValid((uint64_t)fsname))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isStringValid((uint64_t)image))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	if (!isStringValid((uint64_t)mountpoint))
	{
		ERRNO = EFAULT;
		return -1;
	};
	
	Thread *ct = getCurrentThread();
	if (ct->euid != 0)
	{
		ct->therrno = EPERM;
		return -1;
	};

	if (flags != 0)
	{
		ct->therrno = EINVAL;
		return -1;
	};

	if (mountpoint[strlen(mountpoint)-1] != '/')
	{
		ct->therrno = EINVAL;
		return -1;
	};

	semWait(&semFS);
	if (firstDriver == NULL)
	{
		semSignal(&semFS);
		ct->therrno = EINVAL;
		return -1;
	};

	FSDriver *scan = firstDriver;
	while (strcmp(scan->name, fsname) != 0)
	{
		if (scan->next == NULL)
		{
			semSignal(&semFS);
			ct->therrno = EINVAL;
			return -1;
		};
		scan = scan->next;
	};

	FileSystem *fs = (FileSystem*) kmalloc(sizeof(FileSystem));
	memset(fs, 0, sizeof(FileSystem));
	int status = scan->onMount(image, fs, sizeof(FileSystem));
	semSignal(&semFS);

	if (status != 0)
	{
		kfree(fs);
		return -1;
	};

	status = mount(mountpoint, fs, flags);

	if (status != 0)
	{
		kfree(fs);
		return -1;
	};

	return 0;
};
