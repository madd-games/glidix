/*
	Glidix Shell Utilities

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

#ifndef __glidix__
#error	This program only runs under Glidix.
#endif

#include <sys/glidix.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#define	IOCTL_VIDEO_DEVSTAT		_GLIDIX_IOCTL_ARG(LGIDisplayDeviceStat, _GLIDIX_IOCTL_INT_VIDEO, 0)
#define	IOCTL_VIDEO_MODSTAT		_GLIDIX_IOCTL_ARG(LGIDisplayMode, _GLIDIX_IOCTL_INT_VIDEO, 1)
#define	IOCTL_VIDEO_SETMODE		_GLIDIX_IOCTL_ARG(LGIDisplayMode, _GLIDIX_IOCTL_INT_VIDEO, 2)

typedef struct
{
	uint8_t				index;
	int				width;
	int				height;
} LGIDisplayMode;

const char *progName;

void usage()
{
	fprintf(stderr, "USAGE:\t%s <device> <mode-number>\n", progName);
	fprintf(stderr, "\tOpens a graphics terminal on the specified graphics\n");
	fprintf(stderr, "\tdevice and in the given video mode.\n");
};

int main(int argc, char *argv[])
{
	progName = argv[0];
	
	if (geteuid() != 0)
	{
		fprintf(stderr, "%s: you must be root\n", argv[0]);
		return 1;
	};
	
	if (argc != 3)
	{
		usage();
		return 1;
	};
	
	int modeidx;
	if (sscanf(argv[2], "%d", &modeidx) != 1)
	{
		usage();
		return 1;
	};
	
	int fd = open(argv[1], O_RDWR);
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open device '%s': %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	LGIDisplayMode mode;
	mode.index = modeidx;
	if (ioctl(fd, IOCTL_VIDEO_MODSTAT, &mode) != 0)
	{
		fprintf(stderr, "%s: cannot get video mode %d: %s\n", argv[0], modeidx, strerror(errno));
		return 1;
	};
	
	if (ioctl(fd, IOCTL_VIDEO_SETMODE, &mode) != 0)
	{
		fprintf(stderr, "%s: cannot switch to mode %d: %s\n", argv[0], modeidx, strerror(errno));
		return 1;
	};
	
	if (_glidix_kopt(_GLIDIX_KOPT_GFXTERM, 1) != 0)
	{
		fprintf(stderr, "%s: cannot enable gfxterm: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	printf("New terminal size: %dx%d\n", mode.width/9, mode.height/16);
	return 0;
};
