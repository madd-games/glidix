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

#ifndef __glidix_humin_h
#define __glidix_humin_h

/**
 * This file contains data structures and routines which form a uniform interface for all
 * human input devices, such as keyboards, mice, joysticks, touch screens, etc. When a driver
 * detects and registers an input device, a /dev/huminX file (with X being a number) is
 * created, which may be opened ("acquired") by one process at a time. When such a device is
 * not opened, it is used by the kernel for terminal input. Otherwise, the process that opened
 * the file - called the "controlling process", can call read() on the file descriptor to
 * wait for input events, as described below. When the file is closed, it is released back
 * to the system.
 */

#include <glidix/common.h>
#include <glidix/devfs.h>
#include <glidix/semaphore.h>

/**
 * Event types.
 */
#define	HUMIN_EV_BUTTON_UP			1
#define	HUMIN_EV_BUTTON_DOWN			2

/**
 * Standard scancodes. Keyboards output scancodes in the range 0-255.
 * Mouse uses 0x100-0x1FF.
 * Special ones defined here.
 */
#define	HUMIN_SC_MOUSE_LEFT			0x100
#define	HUMIN_SC_MOUSE_MIDDLE			0x101
#define	HUMIN_SC_MOUSE_RIGHT			0x102

/**
 * Describes an input event.
 */
typedef union
{
	/**
	 * Message type, one of the HUMIN_EV_* macros.
	 */
	int					type;
	
	/**
	 * For HUMIN_EV_BUTTON_UP and HUMIN_EV_BUTTON_DOWN.
	 */
	struct
	{
		int				type;
		int				scancode;
	} button;

#if 0	
	/**
	 * For HUMIN_EV_MOTION_RELATIVE and HUMIN_EV_MOTION_ABSOLUTE.
	 */
	struct
	{
		int				type;
		int				x;
		int				y;
	} motion;
#endif
} HuminEvent;

/**
 * Forms the input event queue.
 */
typedef struct HuminEvQueue_
{
	HuminEvent				ev;
	struct HuminEvQueue_*			next;
} HuminEvQueue;

/**
 * Describes a particular human input device.
 */
typedef struct
{
	/**
	 * The semaphore which controlls access to this device.
	 */
	Semaphore				sem;
	
	/**
	 * Event counter.
	 */
	Semaphore				evCount;
	
	/**
	 * A unique ID, used to derive the X in /dev/huminX.
	 */
	int					id;
	
	/**
	 * Human-readable device name, such as "PS/2 Keyboard and Mouse".
	 */
	char					devname[512];
	
	/**
	 * Event queue. The events are only queued while the device is open.
	 */
	HuminEvQueue*				first;
	HuminEvQueue*				last;
	
	/**
	 * The device file that represents this device.
	 */
	Device					dev;
	
	/**
	 * Whether or not the device is currently in use (opened by a process).
	 */
	int					inUse;
	
	/**
	 * This is true while the device still exists.
	 */
	int					active;
} HuminDevice;

HuminDevice*		huminCreateDevice(const char *devname);
void			huminDeleteDevice(HuminDevice *hudev);
void			huminPostEvent(HuminDevice *hudev, HuminEvent *ev);

#endif
