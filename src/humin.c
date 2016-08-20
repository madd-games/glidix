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
#include <glidix/ktty.h>
#include <glidix/term.h>

// US keyboard layout
static unsigned char keymap[128] =
{
		0,	0, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
	'9', '0', '-', '=', '\b',	/* Backspace */
	'\t',			/* Tab */
	'q', 'w', 'e', 'r',	/* 19 */
	't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
		0,			/* 29	 - Control */
	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',	 0x80,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
	'm', ',', '.', '/',	 0x80,				/* Right shift */
	'*',
		0,	/* Alt */
	' ',	/* Space bar */
		0,	/* Caps lock */
		0,	/* 59 - F1 key ... > */
		0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,
		0,	/* < ... F10 */
		0,	/* 69 - Num lock*/
		0,	/* Scroll Lock */
		0,	/* Home key */
		CC_ARROW_UP,	/* Up Arrow */
		0,	/* Page Up */
	'-',
		CC_ARROW_LEFT,	/* Left Arrow */
		0,
		CC_ARROW_RIGHT,	/* Right Arrow */
	'+',
		0,	/* 79 - End key*/
		CC_ARROW_DOWN,	/* Down Arrow */
		0,	/* Page Down */
		0,	/* Insert Key */
		0,	/* Delete Key */
		0,	 0,	 0,
		CC_PANIC,	/* F11 Key */
		0,	/* F12 Key */
		0,	/* All other keys are undefined */
};

// when shift is pressed
static unsigned char keymapShift[128] =
{
		0,	0, '!', '@', '#', '$', '%', '^', '&', '*',	/* 9 */
	'(', ')', '_', '+', '\b',	/* Backspace */
	'\t',			/* Tab */
	'Q', 'W', 'E', 'R',	/* 19 */
	'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',	/* Enter key */
		0,			/* 29	 - Control */
	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',	/* 39 */
 '\"', '`',	 0x80,		/* Left shift */
 '\\', 'Z', 'X', 'C', 'V', 'B', 'N',			/* 49 */
	'M', '<', '>', '?',	 0x80,				/* Right shift */
	'*',
		0,	/* Alt */
	' ',	/* Space bar */
		0,	/* Caps lock */
		0,	/* 59 - F1 key ... > */
		0,	 0,	 0,	 0,	 0,	 0,	 0,	 0,
		0,	/* < ... F10 */
		0,	/* 69 - Num lock*/
		0,	/* Scroll Lock */
		0,	/* Home key */
		0,	/* Up Arrow */
		0,	/* Page Up */
	'-',
		0,	/* Left Arrow */
		0,
		0,	/* Right Arrow */
	'+',
		0,	/* 79 - End key*/
		0,	/* Down Arrow */
		0,	/* Page Down */
		0,	/* Insert Key */
		0,	/* Delete Key */
		0,	 0,	 0,
		0,	/* F11 Key */
		0,	/* F12 Key */
		0,	/* All other keys are undefined */
};

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
	if (semWaitGen(&hudev->evCount, 1, SEM_W_INTR, 0) < 0)
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

static int ctrl = 0;
static int shift = 0;

void huminPostEvent(HuminDevice *hudev, HuminEvent *ev)
{
	semWait(&hudev->sem);
	
	// handle forced panic key
	if (ev->type == HUMIN_EV_BUTTON_DOWN)
	{
		if ((ev->button.scancode > 0) && (ev->button.scancode < 128))
		{
			if (keymap[ev->button.scancode] == CC_PANIC)
			{
				panic("user forced panic (F11)");
			};
		};
	};
	
	if (!hudev->inUse)
	{
		semSignal(&hudev->sem);
		if ((ev->type == HUMIN_EV_BUTTON_DOWN) || (ev->type == HUMIN_EV_BUTTON_UP))
		{
			if (ev->button.scancode == 0x1D)
			{
				if (ev->type == HUMIN_EV_BUTTON_DOWN) ctrl = 1;
				else ctrl = 0;
			}
			else
			{
				unsigned char key = keymap[ev->button.scancode];
				if (shift) key = keymapShift[ev->button.scancode];
				if (key == 0x80)
				{
					if (ev->type == HUMIN_EV_BUTTON_DOWN) shift = 1;
					else shift = 0;
					return;
				};
				if (key == 0) return;

				if (ev->type == HUMIN_EV_BUTTON_DOWN)
				{
					
					if (ctrl)
					{
						if (key == 'c')
						{
							termPutChar(CC_VINTR);
						};
					}
					else
					{
						termPutChar(key);
					};
				};
			};
		};
		
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
