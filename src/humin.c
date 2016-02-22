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

#include <glidix/common.h>
#include <glidix/humin.h>
#include <glidix/devfs.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/console.h>

static int huminNextID = 0;

static void hudev_close(File *fp)
{
	HuminDevice *hudev = (HuminDevice*) fp->fsdata;
	semWait(&hudev->sem);
	hudev->inUse = 0;
	
	HuminEvQueue *ev = hudev->first;
	while (ev != NULL)
	{
		HuminEvQueue *next = ev->next;
		kfree(ev);
		ev = next;
	};
	hudev->first = NULL;
	
	if (!hudev->active)
	{
		DeleteDevice(hudev->dev);
	}
	else
	{
		semSignal(&hudev->sem);
	};
};

static ssize_t hudev_read(File *fp, void *buffer, size_t size)
{
	if (size > sizeof(HuminEvent)) size = sizeof(HuminEvent);
	
	HuminDevice *hudev = (HuminDevice*) fp->fsdata;
	if (semWaitTimeout(&hudev->evCount, 1, 0) < 0)
	{
		ERRNO = EINTR;
		return -1;
	};
	
	semWait(&hudev->sem);
	HuminEvQueue *qel = hudev->first;
	hudev->first = qel->next;
	semSignal(&hudev->sem);
	
	memcpy(buffer, &qel->ev, size);
	kfree(qel);
	return (ssize_t) size;
};

static int hudev_open(void *data, File *fp, size_t szfile)
{
	HuminDevice *hudev = (HuminDevice*) data;
	semWait(&hudev->sem);
	if (hudev->inUse)
	{
		semSignal(&hudev->sem);
		return VFS_BUSY;
	};
	
	if (!hudev->active)
	{
		semSignal(&hudev->sem);
		return VFS_BUSY;
	};
	
	hudev->inUse = 1;
	semSignal(&hudev->sem);
	
	fp->fsdata = hudev;
	fp->read = hudev_read;
	fp->close = hudev_close;
	return 0;
};

HuminDevice* huminCreateDevice(const char *devname)
{
	HuminDevice *hudev = NEW(HuminDevice);
	semInit(&hudev->sem);
	semWait(&hudev->sem);
	semInit2(&hudev->evCount, 0);
	
	hudev->id = __sync_fetch_and_add(&huminNextID, 1);
	strcpy(hudev->devname, devname);
	hudev->first = NULL;
	hudev->last = NULL;
	
	char filename[256];
	strformat(filename, 256, "humin%d", hudev->id);
	hudev->dev = AddDevice(filename, hudev, hudev_open, 0600);

	hudev->inUse = 0;
	hudev->active = 1;
	
	semSignal(&hudev->sem);
	return hudev;
};

void huminDeleteDevice(HuminDevice *hudev)
{
	kprintf_debug("huminDeleteDevice\n");
	semWait(&hudev->sem);
	
	if (!hudev->inUse)
	{
		HuminEvQueue *ev = hudev->first;
		while (ev != NULL)
		{
			HuminEvQueue *next = ev->next;
			kfree(ev);
			ev = next;
		};
		
		// also frees hudev
		DeleteDevice(hudev->dev);
	}
	else
	{
		hudev->active = 0;
		semSignal(&hudev->sem);
	};
};

void huminPostEvent(HuminDevice *hudev, HuminEvent *ev)
{
	semWait(&hudev->sem);
	if (!hudev->inUse)
	{
		// TODO: process keys for terminal and stuff
		semSignal(&hudev->sem);
		return;
	};
	
	HuminEvQueue *qel = NEW(HuminEvQueue);
	memcpy(&qel->ev, ev, sizeof(HuminEvent));
	qel->next = NULL;
	
	if (hudev->first == NULL)
	{
		hudev->first = hudev->last = qel;
	}
	else
	{
		hudev->last->next = qel;
		hudev->last = qel;
	};
	
	semSignal(&hudev->evCount);
	semSignal(&hudev->sem);
};
