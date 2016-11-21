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

#ifndef __glidix_video_h
#define __glidix_video_h

#include <glidix/common.h>
#include <glidix/ioctl.h>
#include <glidix/devfs.h>

#define	IOCTL_VIDEO_DEVSTAT		IOCTL_ARG(LGIDisplayDeviceStat, IOCTL_INT_VIDEO, 0)
#define	IOCTL_VIDEO_MODSTAT		IOCTL_ARG(LGIDisplayMode, IOCTL_INT_VIDEO, 1)
#define	IOCTL_VIDEO_SETMODE		IOCTL_ARG(LGIDisplayMode, IOCTL_INT_VIDEO, 2)

typedef struct
{
	uint8_t				numModes;
} LGIDisplayDeviceStat;

typedef struct
{
	uint8_t				index;
	int				width;
	int				height;
} LGIDisplayMode;

typedef struct _LGIDeviceInterface
{
	/**
	 * Private, driver-specific data.
	 */
	void*				drvdata;

	/**
	 * The device (devfs) handle for this interface.
	 * Drivers should not use this.
	 */
	Device				dev;

	/**
	 * Information about the device.
	 */
	LGIDisplayDeviceStat		dstat;

	/**
	 * Get information about the specified video mode.
	 * Returns 0 on success, -1 on device error.
	 * The 'index' field is already set.
	 */
	int (*getModeInfo)(struct _LGIDeviceInterface *intf, LGIDisplayMode *mode);

	/**
	 * Set the video mode.
	 */
	void (*setMode)(struct _LGIDeviceInterface *intf, LGIDisplayMode *mode);

	/**
	 * Draw the software console buffer to the screen. The buffer is always in the VGA
	 * format (but color may be ignored for the rendering). This must not call any memory allocation
	 * functions etc, as it may be called during a panic.
	 */
	void (*renderConsole)(struct _LGIDeviceInterface *intf, const unsigned char *buffer, int width, int height);
	
	/**
	 * Swap a framebuffer to the screen.
	 */
	void (*write)(struct _LGIDeviceInterface *intf, const void *data, size_t len);

	/**
	 * Get current video memory.
	 */
	FrameList* (*getFramebuffer)(struct _LGIDeviceInterface *intf, size_t len, int prot, int flags, off_t offset);
} LGIDeviceInterface;

void lgiKInit();

/**
 * Add a new LGI device. This is used by drivers to report the existence of a display device.
 * Returns 0 on success, -1 on failure.
 */
int lgiKAddDevice(const char *name, LGIDeviceInterface *intf);

/**
 * Called after a panic to display the console.
 */
void lgiRenderConsole(const unsigned char *buffer, int width, int height);

#endif
