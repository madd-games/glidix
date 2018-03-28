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

#ifndef __glidix_video_h
#define __glidix_video_h

#include <glidix/common.h>
#include <glidix/ioctl.h>
#include <glidix/devfs.h>

#define	IOCTL_VIDEO_MODESET			IOCTL_ARG(VideoModeRequest, IOCTL_INT_VIDEO, 1)
#define	IOCTL_VIDEO_GETINFO			IOCTL_ARG(VideoInfo, IOCTL_INT_VIDEO, 2)

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
 * Graphics information structure - compatible with DDI (see libddi.h)
 */
typedef struct
{
	/**
	 * Name of the renderer client library, e.g. "softrender".
	 */
	char					renderer[32];
	
	/**
	 * Number of presentable surfaces that may be allocated. Must be at least 1.
	 */
	int					numPresentable;
	
	/**
	 * Bitset of supported features.
	 */
	int					features;
	
	/**
	 * Reserved.
	 */
	int					resv[16];
} VideoInfo;

/**
 * Request for video mode setting (IOCTL).
 */
typedef struct
{
	/**
	 * (In) Requested resolution (see VIDEO_RES_* macros above).
	 * (Out) Actual resolution set (use VIDEO_RES_WIDTH() and VIDEO_RES_HEIGHT() to extract values)
	 */
	uint64_t				res;
	
	/**
	 * (Out) The pixel format of the display. The framebuffer is placed at virtual offset 0.
	 */
	VideoPixelFormat			format;
} VideoModeRequest;

/**
 * Driver operations.
 */
struct VideoDisplay_;
typedef struct
{
	/**
	 * Video mode setting. Returns 0 on success, -1 on error (invalid options).
	 */
	int (*setmode)(struct VideoDisplay_ *display, VideoModeRequest *req);
	
	/**
	 * Get device info.
	 */
	void (*getinfo)(struct VideoDisplay_ *display, VideoInfo *info);
	
	/**
	 * Return the physical frame number corresponding to the page-aligned position 'pos' in video
	 * memory (this is the offset used with mmap()). Return 0 if no such page exists.
	 */
	uint64_t (*getpage)(struct VideoDisplay_ *display, off_t off);
	
	/**
	 * Exit graphics mode. This function must return to VGA-compatible 80x25 text mode.
	 */
	void (*exitmode)(struct VideoDisplay_ *display);
} VideoOps;

/**
 * Describes a display.
 */
typedef struct VideoDisplay_
{
	/**
	 * Driver-specific data.
	 */
	void*					data;
	
	/**
	 * Operations.
	 */
	VideoOps*				ops;
	
	/**
	 * The device file representing this display.
	 */
	Inode*					dev;
	char*					devname;
	
	/**
	 * The file handle which set the mode on this device.
	 */
	File*					fpModeSetter;
} VideoDisplay;

/**
 * Describes a driver. Used mainly for display numbering.
 */
typedef struct
{
	const char*				name;
	int					nextNum;
} VideoDriver;

/**
 * Create a video driver object.
 */
VideoDriver *videoCreateDriver(const char *name);

/**
 * Delete a video driver object. This does NOT delete the displays associated with it, and you may
 * delete them later.
 */
void videoDeleteDriver(VideoDriver *drv);

/**
 * Create a new display object. 'data' is an arbitrary pointer set by the driver, and 'ops' points to a filled-in
 * VideoOps structure. 'dr'v is the video driver to which this display is attached.
 */
VideoDisplay* videoCreateDisplay(VideoDriver *drv, void *data, VideoOps *ops);

/**
 * Delete a display object.
 */
void videoDeleteDisplay(VideoDisplay *disp);

#endif
