/*
	Glidix kernel

	Copyright (c) 2014-2015, Madd Games.
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

#include <glidix/video.h>
#include <glidix/devfs.h>
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/string.h>
#include <glidix/memory.h>
#include <glidix/console.h>

typedef struct
{
	LGIDeviceInterface*			intf;
} DisplayData;

static int lgi_ioctl(File *fp, uint64_t cmd, void *argp)
{
	//LGIDeviceInterface *intf = (LGIDeviceInterface*) fp->fsdata;
	DisplayData *data = (DisplayData*) fp->fsdata;
	LGIDeviceInterface *intf = data->intf;
	Thread *ct = getCurrentThread();

	switch (cmd)
	{
	case IOCTL_VIDEO_DEVSTAT:
		memcpy(argp, &intf->dstat, sizeof(LGIDisplayDeviceStat));
		return 0;
	case IOCTL_VIDEO_MODSTAT:
		ct->therrno = EIO;
		return intf->getModeInfo(intf, (LGIDisplayMode*) argp);
	case IOCTL_VIDEO_SETMODE:
		intf->setMode(intf, (LGIDisplayMode*) argp);
		return 0;
	default:
		ct->therrno = EINVAL;
		return -1;
	};
};

static void lgi_close(File *fp)
{
	DisplayData *data = (DisplayData*) fp->fsdata;
	kfree(data->intf);
	kfree(data);
};

static ssize_t lgi_write(File *fp, const void *data, size_t size)
{
	DisplayData *disp = (DisplayData*) fp->fsdata;
	disp->intf->write(disp->intf, data, size);
	return (ssize_t) size;
};

FrameList* lgi_mmap(File *fp, size_t len, int prot, int flags, off_t offset)
{
	DisplayData *disp = (DisplayData*) fp->fsdata;
	return disp->intf->getFramebuffer(disp->intf, len, prot, flags, offset);
};

static int lgi_open(void *data, File *fp, size_t szFile)
{
	DisplayData *disp = (DisplayData*) kmalloc(sizeof(DisplayData));
	disp->intf = (LGIDeviceInterface*) data;

	fp->fsdata = disp;
	fp->ioctl = lgi_ioctl;
	fp->close = lgi_close;
	fp->write = lgi_write;
	fp->mmap = lgi_mmap;

	return 0;
};

int lgiKAddDevice(const char *name, LGIDeviceInterface *intf)
{
	intf->dev = AddDevice(name, intf, lgi_open, 0);
	if (intf->dev == NULL)
	{
		return -1;
	};

	return 0;
};
