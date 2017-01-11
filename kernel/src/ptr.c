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

#include <glidix/ptr.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/errno.h>
#include <glidix/sched.h>

static void ptrDownref(PointDevice *ptr)
{
	if (__sync_and_and_fetch(&ptr->refcount, -1) == 0)
	{
		kfree(ptr);
	};
};

static void ptrdev_close(File *fp)
{
	PointDevice *ptr = (PointDevice*) fp->fsdata;
	ptrDownref(ptr);
};

static void ptrdev_pollinfo(File *fp, Semaphore **sems)
{
	PointDevice *ptr = (PointDevice*) fp->fsdata;
	sems[PEI_READ] = &ptr->semUpdate;
	sems[PEI_WRITE] = vfsGetConstSem();
};

static ssize_t ptrdev_read(File *fp, void *buffer, size_t size)
{
	if (size != sizeof(PtrState))
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	PointDevice *ptr = (PointDevice*) fp->fsdata;
	int status = semWaitGen(&ptr->semUpdate, 1, SEM_W_FILE(fp->oflag), 0);
	if (status < 0)
	{
		ERRNO = -status;
		return -1;
	};
	
	semWaitGen(&ptr->semUpdate, -1, 0, 0);
	
	semWait(&ptr->lock);
	memcpy(buffer, &ptr->state, sizeof(PtrState));
	semSignal(&ptr->lock);
	
	return sizeof(PtrState);
};

static ssize_t ptrdev_write(File *fp, const void *buffer, size_t size)
{
	if (size != sizeof(PtrState))
	{
		ERRNO = EINVAL;
		return -1;
	};
	
	PointDevice *ptr = (PointDevice*) fp->fsdata;
	semWait(&ptr->lock);
	memcpy(&ptr->state, buffer, sizeof(PtrState));
	semSignal(&ptr->lock);
	semSignal(&ptr->semUpdate);
	
	return sizeof(PtrState);
};

static int ptrdev_open(void *context, File *fp, size_t szfile)
{
	PointDevice *ptr = (PointDevice*) context;
	__sync_fetch_and_add(&ptr->refcount, 1);
	
	fp->fsdata = ptr;
	fp->write = ptrdev_write;
	fp->read = ptrdev_read;
	fp->close = ptrdev_close;
	fp->pollinfo = ptrdev_pollinfo;
	return 0;
};

static int nextPtrID = 0;
PointDevice *ptrCreate()
{
	PointDevice *ptr = NEW(PointDevice);
	if (ptr == NULL)
	{
		return NULL;
	};
	
	ptr->state.width = 0;
	ptr->state.height = 0;
	ptr->state.posX = 0;
	ptr->state.posY = 0;
	
	semInit2(&ptr->semUpdate, 0);
	semInit(&ptr->lock);
	
	ptr->refcount = 1;
	
	char devname[256];
	strformat(devname, 256, "ptr%d", __sync_fetch_and_add(&nextPtrID, 1));
	
	ptr->dev = AddDevice(devname, ptr, ptrdev_open, 0600 | VFS_MODE_CHARDEV);
	if (ptr->dev == NULL)
	{
		kfree(ptr);
		return NULL;
	};
	
	return ptr;
};

void ptrDelete(PointDevice *ptr)
{
	DeleteDevice(ptr->dev);
	ptr->dev = NULL;
	ptrDownref(ptr);
};

void ptrRelMotion(PointDevice *ptr, int deltaX, int deltaY)
{
	semWait(&ptr->lock);
	if ((ptr->state.width != 0) && (ptr->state.height != 0))
	{
		int newX = ptr->state.posX + deltaX;
		int newY = ptr->state.posY + deltaY;
		
		if (newX < 0) newX = 0;
		if (newY < 0) newY = 0;
		
		if (newX >= ptr->state.width) newX = ptr->state.width - 1;
		if (newY >= ptr->state.height) newY = ptr->state.height - 1;
		
		ptr->state.posX = newX;
		ptr->state.posY = newY;
	};
	semSignal(&ptr->lock);
	semSignal(&ptr->semUpdate);
};
