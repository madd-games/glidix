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

#ifndef __glidix_video_h
#define __glidix_video_h

#include <glidix/common.h>
#include <glidix/ioctl.h>

#define	IOCTL_VIDEO_DEVSTAT		IOCTL_ARG(KDisplayDeviceStat, IOCTL_INT_VIDEO, 0)
#define	IOCTL_VIDEO_MODSTAT		IOCTL_ARG(KDisplayMode, IOCTL_INT_VIDEO, 1)
#define	IOCTL_VIDEO_SETMODE		IOCTL_ARG(KDisplayMode, IOCTL_INT_VIDEO, 2)
#define	IOCTL_VIDEO_POST		IOCTL_NOARG(IOCTL_INT_VIDEO, 3)

typedef struct
{
	unsigned int			index;
	unsigned int			width;
	unsigned int			height;
	uint64_t			fbBytes;
	uint8_t				bytesPerPixel;
	uint8_t				redIndex;
	uint8_t				greenIndex;
	uint8_t				blueIndex;
} KDisplayMode;

typedef struct
{
	char				devName[256];
	unsigned int			numModes;
	char				libDriver[256];
} KDisplayDeviceStat;

#endif
