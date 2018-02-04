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
#include <glidix/humin.h>
#include <glidix/devfs.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/sched.h>
#include <glidix/errno.h>
#include <glidix/console.h>
#include <glidix/ktty.h>
#include <glidix/term.h>

static int huminNextID = 0;

static ssize_t hudev_read(Inode *inode, File *fp, void *buffer, size_t size, off_t off)
{
	if (size > sizeof(HuminEvent)) size = sizeof(HuminEvent);
	
	HuminDevice *hudev = (HuminDevice*) inode->fsdata;
	if (semWaitGen(&hudev->evCount, 1, SEM_W_FILE(fp->oflags), 0) < 0)
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

static void hudev_pollinfo(Inode *inode, File *fp, Semaphore **sems)
{
	HuminDevice *hudev = (HuminDevice*) inode->fsdata;
	sems[PEI_READ] = &hudev->evCount;
};

static void* hudev_open(Inode *inode, int oflags)
{
	HuminDevice *hudev = (HuminDevice*) inode->fsdata;
	
	semWait(&hudev->sem);
	if (hudev->inUse)
	{
		semSignal(&hudev->sem);
		ERRNO = EBUSY;
		return NULL;
	}
	else
	{
		hudev->inUse = 1;
		semSignal(&hudev->sem);
		return hudev;
	};
};

static void hudev_close(Inode *inode, void *filedata)
{
	HuminDevice *hudev = (HuminDevice*) filedata;
	semWait(&hudev->sem);
	hudev->inUse = 0;
	semSignal(&hudev->sem);
};

static void hudev_free(Inode *inode)
{
	kfree(inode->fsdata);
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
	//hudev->dev = AddDevice(filename, hudev, hudev_open, 0600);
	hudev->inode = vfsCreateInode(NULL, 0600);
	hudev->inode->fsdata = hudev;
	hudev->inode->free = hudev_free;
	hudev->inode->open = hudev_open;
	hudev->inode->close = hudev_close;
	hudev->inode->pread = hudev_read;
	hudev->inode->pollinfo = hudev_pollinfo;
	hudev->iname = strdup(filename);
	
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
		devfsRemove(hudev->iname);
		kfree(hudev->iname);
		vfsDownrefInode(hudev->inode);
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
	
	if (!hudev->inUse)
	{
		semSignal(&hudev->sem);
		if (ev->type == HUMIN_EV_BUTTON_DOWN)
		{
			int keycode = ev->button.keycode;
			if ((keycode == HUMIN_KC_LCTRL) || (keycode == HUMIN_KC_RCTRL))
			{
				ctrl = 1;
			}
			else if ((keycode == HUMIN_KC_LSHIFT) || (keycode == HUMIN_KC_RSHIFT))
			{
				shift = 1;
			}
			else if (ctrl)
			{
				if (keycode == HUMIN_KC_C)
				{
					termPutChar(CC_VINTR);
				};
			}
			else
			{
				// control not pressed
				if (keycode == HUMIN_KC_DOWN)
				{
					termPutChar(CC_ARROW_DOWN);
				}
				else if (keycode == HUMIN_KC_UP)
				{
					termPutChar(CC_ARROW_UP);
				}
				else if (keycode == HUMIN_KC_LEFT)
				{
					termPutChar(CC_ARROW_LEFT);
				}
				else if (keycode == HUMIN_KC_RIGHT)
				{
					termPutChar(CC_ARROW_RIGHT);
				}
				else if (keycode == HUMIN_KC_RETURN)
				{
					termPutChar('\n');
				}
				else if ((keycode >= '0') && (keycode <= '9'))
				{
					if (shift)
					{
						termPutChar(")!@#$%^&*("[keycode-'0']);
					}
					else
					{
						termPutChar((char) keycode);
					};
				}
				else if (keycode == '-')
				{
					if (shift) termPutChar('_');
					else termPutChar('-');
				}
				else if (keycode == '=')
				{
					if (shift) termPutChar('+');
					else termPutChar('=');
				}
				else if (keycode == '`')
				{
					if (shift) termPutChar('~');
					else termPutChar('`');
				}
				else if (keycode == '\t')
				{
					termPutChar('\t');
				}
				else if (keycode == '[')
				{
					if (shift) termPutChar('{');
					else termPutChar('[');
				}
				else if (keycode == ']')
				{
					if (shift) termPutChar('}');
					else termPutChar(']');
				}
				else if (keycode == ';')
				{
					if (shift) termPutChar(':');
					else termPutChar(';');
				}
				else if (keycode == '\'')
				{
					if (shift) termPutChar('"');
					else termPutChar('\'');
				}
				else if (keycode == '\\')
				{
					if (shift) termPutChar('|');
					else termPutChar('\\');
				}
				else if (keycode == '<')
				{
					if (shift) termPutChar('>');
					else termPutChar('<');
				}
				else if (keycode == ',')
				{
					if (shift) termPutChar('<');
					else termPutChar(',');
				}
				else if (keycode == '.')
				{
					if (shift) termPutChar('>');
					else termPutChar('.');
				}
				else if (keycode == '/')
				{
					if (shift) termPutChar('?');
					else termPutChar('/');
				}
				else if ((keycode >= 'a') && (keycode <= 'z'))
				{
					if (shift) termPutChar((char) (keycode-'a'+'A'));
					else termPutChar((char) keycode);
				}
				else if (keycode == '\b')
				{
					termPutChar('\b');
				}
				else if (keycode == ' ')
				{
					termPutChar(' ');
				};
			};
		}
		else if (ev->type == HUMIN_EV_BUTTON_UP)
		{
			int keycode = ev->button.keycode;
			if ((keycode == HUMIN_KC_LCTRL) || (keycode == HUMIN_KC_RCTRL))
			{
				ctrl = 0;
			}
			else if ((keycode == HUMIN_KC_LSHIFT) || (keycode == HUMIN_KC_RSHIFT))
			{
				shift = 0;
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
