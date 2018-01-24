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

#include <glidix/common.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/video.h>
#include <glidix/devfs.h>
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/console.h>

typedef struct
{
	VideoDisplay *disp;
} VideoDeviceData;

VideoDriver *videoCreateDriver(const char *name)
{
	VideoDriver *drv = NEW(VideoDriver);
	if (drv == NULL) return NULL;
	
	drv->name = name;
	drv->nextNum = 0;
	
	return drv;
};

void videoDeleteDriver(VideoDriver *drv)
{
	kfree(drv);
};

#if 0
void videoDisplayDownref(VideoDisplay *disp)
{
	if (__sync_add_and_fetch(&disp->refcount, -1) == 0)
	{
		kfree(disp);
	};
};
#endif

#if 0
void video_close(File *fp)
{
	VideoDisplay *disp = fp->fsdata;
	if (disp->fpModeSetter == fp)
	{
		disp->fpModeSetter = NULL;
		disp->ops->exitmode(disp);
		enableConsole();
	};
	videoDisplayDownref(disp);
};
#endif

void video_free(Inode *inode)
{
	kfree(inode->fsdata);
};

uint64_t video_getpage(FileTree *ft, off_t pos)
{
	//uint64_t base = (uint64_t) ft->data;
	//if (pos > ft->size) return 0;
	//return (base + pos) >> 12;
	
	VideoDisplay *disp = (VideoDisplay*) ft->data;
	return disp->ops->getpage(disp, pos);
};

int video_ioctl(Inode *inode, File *fp, uint64_t cmd, void *argp)
{
	VideoDisplay *disp = inode->fsdata;
	
	switch (cmd)
	{
	case IOCTL_VIDEO_MODESET:
		if (!havePerm(XP_DISPCONF))
		{
			ERRNO = EPERM;
			return -1;
		};
		disableConsole();
		if (disp->ops->setmode(disp, (VideoModeRequest*)argp) == 0)
		{
			disp->fpModeSetter = fp;
			return 0;
		};
		enableConsole();
		return -1;
	case IOCTL_VIDEO_GETINFO:
		disp->ops->getinfo(disp, (VideoInfo*) argp);
		return 0;
	default:
		ERRNO = ENODEV;
		return -1;
	};
};

#if 0
int video_open(void *data, File *fp, size_t szFile)
{
	VideoDeviceData *vdata = (VideoDeviceData*) data;
	VideoDisplay *disp = vdata->disp;
	
	__sync_fetch_and_add(&disp->refcount, 1);
	fp->fsdata = disp;
	fp->ioctl = video_ioctl;
	fp->tree = video_tree;
	fp->close = video_close;
	
	return 0;
};
#endif

VideoDisplay* videoCreateDisplay(VideoDriver *drv, void *data, VideoOps *ops)
{
	VideoDisplay *disp = NEW(VideoDisplay);
	if (disp == NULL) return NULL;
	
	disp->data = data;
	disp->ops = ops;
	disp->fpModeSetter = NULL;
	
	char devname[64];
	strformat(devname, 64, "%s%d", drv->name, __sync_fetch_and_add(&drv->nextNum, 1));
	
	VideoDeviceData *devdata = NEW(VideoDeviceData);
	if (devdata == NULL)
	{
		kfree(disp);
		return NULL;
	};
	
	devdata->disp = disp;
	
	Inode *inode = vfsCreateInode(NULL, 0644);
	inode->fsdata = devdata;
	inode->ioctl = video_ioctl;
	inode->free = video_free;
	inode->ft = ftCreate(FT_ANON);
	inode->ft->data = disp;
	inode->ft->getpage = video_getpage;
	
	disp->dev = inode;
	disp->devname = strdup(devname);
	
	if (devfsAdd(devname, inode) != 0)
	{
		kfree(data);
		kfree(disp->devname);
		kfree(disp);
		return NULL;
	};

	return disp;
};

void videoDeleteDisplay(VideoDisplay *disp)
{
	devfsRemove(disp->devname);
	kfree(disp->devname);
};
