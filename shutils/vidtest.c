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

#include <sys/glidix.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <fcntl.h>

#define	IOCTL_VIDEO_MODESET			_GLIDIX_IOCTL_ARG(VideoModeRequest, _GLIDIX_IOCTL_INT_VIDEO, 1)
#define	IOCTL_VIDEO_GETFLAGS			_GLIDIX_IOCTL_NOARG(_GLIDIX_IOCTL_INT_VIDEO, 2)

/**
 * Resolution specifications (bottom 8 bits are the setting type).
 */
#define	VIDEO_RES_AUTO				0		/* best resolution for screen (or safe if not detected) */
#define	VIDEO_RES_SAFE				1		/* safe resolution (any availabe safe resolution, e.g. 720x480) */
#define	VIDEO_RES_SPECIFIC(w, h)		(2 | ((w) << 8) | ((h) << 32))

/**
 * Macros to detect exact resolution specification, and extract width and height.
 */
#define	VIDEO_RES_IS_EXACT(s)			(((s) & 0xFF) == 2)
#define	VIDEO_RES_WIDTH(s)			(((s) >> 8) & 0xFFFF)
#define	VIDEO_RES_HEIGHT(s)			(((s) >> 32) & 0xFFFF)

/**
 * Pixel format description - compatible with DDI (see libddi.h)
 */
typedef struct
{
	int					bpp;
	uint32_t				redMask;
	uint32_t				greenMask;
	uint32_t				blueMask;
	uint32_t				alphaMask;
	unsigned int				pixelSpacing;
	unsigned int				scanlineSpacing;
} VideoPixelFormat;

/**
 * Request for video mode setting (IOCTL).
 */
typedef struct
{
	/**
	 * (In) Requested resolution (see VIDEO_RES_* macros above).
	 */
	uint64_t				res;
	
	/**
	 * (Out) The pixel format of the display. The framebuffer is placed at virtual offset 0.
	 */
	VideoPixelFormat			format;
} VideoModeRequest;

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s <display-device>\n", argv[0]);
		fprintf(stderr, "\tTests the specified display device.\n");
		return 1;
	};
	
	int fd = open(argv[1], O_RDWR);
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	VideoModeRequest req;
	req.res = VIDEO_RES_SPECIFIC(640UL, 480UL);
	
	if (ioctl(fd, IOCTL_VIDEO_MODESET, &req) != 0)
	{
		fprintf(stderr, "%s: cannot set video mode: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	void *fbuf = mmap(NULL, 640 * 480 * req.format.bpp, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (fbuf == MAP_FAILED)
	{
		fprintf(stderr, "%s: cannot map framebuffer: %s\n", argv[0], strerror(errno));
		return 1;
	};
	
	fprintf(stderr, "WIDTH: %lu, HEIGHT: %lu, BPP: %d\n", VIDEO_RES_WIDTH(req.res), VIDEO_RES_HEIGHT(req.res), req.format.bpp);
	memset(fbuf, 0xFF, 640 * 240 * req.format.bpp);
	close(fd);
	
	return 0;
};
