/*
	Glidix GUI

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

#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <libgwm.h>
#include <errno.h>

static int guiPid;
static int guiFD;
static int queueFD;
static volatile int initFinished = 0;
static uint64_t nextWindowID;
static uint64_t nextSeq;
static pthread_t listenThread;
static DDIFont *defaultFont;

typedef struct GWMWaiter_
{
	struct GWMWaiter_*		prev;
	struct GWMWaiter_*		next;
	uint64_t			seq;
	GWMMessage			resp;
	pthread_spinlock_t		lock;
} GWMWaiter;

typedef struct EventBuffer_
{
	struct EventBuffer_*		prev;
	struct EventBuffer_*		next;
	GWMEvent			payload;
} EventBuffer;

static pthread_mutex_t waiterLock;
static GWMWaiter* waiters = NULL;
static int eventCounterFD;
static pthread_mutex_t eventLock;
static EventBuffer *firstEvent;
static EventBuffer *lastEvent;
static GWMHandlerInfo *firstHandler = NULL;

DDIColor gwmColorSelection = {0, 0xAA, 0, 0xFF};

static void gwmPostWaiter(uint64_t seq, GWMMessage *resp, const GWMCommand *cmd)
{
	GWMWaiter *waiter = (GWMWaiter*) malloc(sizeof(GWMWaiter));
	waiter->prev = NULL;
	waiter->next = NULL;
	waiter->seq = seq;
	pthread_spin_unlock(&waiter->lock);
	pthread_spin_lock(&waiter->lock);
	
	pthread_mutex_lock(&waiterLock);
	if (_glidix_mqsend(queueFD, guiPid, guiFD, cmd, sizeof(GWMCommand)) != 0)
	{
		pthread_mutex_unlock(&waiterLock);
		perror("_glidix_mqsend");
	};
	waiter->next = waiters;
	waiter->prev = NULL;
	waiters = waiter;
	pthread_mutex_unlock(&waiterLock);
	
	// wait for response
	pthread_spin_lock(&waiter->lock);
	memcpy(resp, &waiter->resp, sizeof(GWMMessage));
	free(waiter);
};

static void* listenThreadFunc(void *ignore)
{
	(void)ignore;
	queueFD = _glidix_mqclient(guiPid, guiFD);
	char msgbuf[65536];
	
	// block all signals. we don't want signal handlers to be invoked in the GWM
	// listening thread, because if the application is buggy it might create undebuggable
	// chaos.
	sigset_t sigset;
	sigfillset(&sigset);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);
	
	initFinished = 1;
	while (1)
	{
		_glidix_msginfo info;
		ssize_t size = _glidix_mqrecv(queueFD, &info, msgbuf, 65536);
		if (size == -1)
		{
			if (errno == EINTR) continue;
			else break;
		};

		if (info.type == _GLIDIX_MQ_INCOMING)
		{
			if (size < sizeof(GWMMessage))
			{
				continue;
			};
		
			GWMMessage *msg = (GWMMessage*) msgbuf;
			if (msg->generic.seq != 0)
			{
				pthread_mutex_lock(&waiterLock);
				GWMWaiter *waiter;
				for (waiter=waiters; waiter!=NULL; waiter=waiter->next)
				{
					if (waiter->seq == msg->generic.seq)
					{
						memcpy(&waiter->resp, msg, sizeof(GWMMessage));
						if (waiter->prev != NULL) waiter->prev->next = waiter->next;
						if (waiter->next != NULL) waiter->next->prev = waiter->prev;
						if (waiter->prev == NULL) waiters = waiter->next;
						pthread_spin_unlock(&waiter->lock);
						break;
					};
				};
				pthread_mutex_unlock(&waiterLock);
			}
			else if (msg->generic.type == GWM_MSG_EVENT)
			{
				EventBuffer *buf = (EventBuffer*) malloc(sizeof(EventBuffer));
				memcpy(&buf->payload, &msg->event.payload, sizeof(GWMEvent));
				
				pthread_mutex_lock(&eventLock);
				if (firstEvent == NULL)
				{
					buf->prev = buf->next = NULL;
					firstEvent = lastEvent = buf;
				}
				else
				{
					buf->prev = lastEvent;
					buf->next = NULL;
					lastEvent->next = buf;
					lastEvent = buf;
				};
				pthread_mutex_unlock(&eventLock);
				int one = 1;
				ioctl(eventCounterFD, _GLIDIX_IOCTL_SEMA_SIGNAL, &one);
			};
		};
	};
	
	return NULL;
};

int gwmInit()
{
	FILE *fp = fopen("/usr/share/gui.pid", "rb");
	if (fp == NULL) return -1;
	if (fscanf(fp, "%d.%d", &guiPid, &guiFD) != 2)
	{
		return -1;
	};
	fclose(fp);
	
	nextWindowID = 1;
	nextSeq = 1;
	eventCounterFD = _glidix_thsync(1, 0);	// semaphore, start at zero
	
	pthread_mutex_init(&waiterLock, NULL);
	pthread_mutex_init(&eventLock, NULL);
	
	if (pthread_create(&listenThread, NULL, listenThreadFunc, NULL) != 0)
	{
		return -1;
	};
	
	while (!initFinished) __sync_synchronize();
	const char *errmsg;
	defaultFont = ddiLoadFont("DejaVu Sans", 12, DDI_STYLE_REGULAR, &errmsg);
	if (defaultFont == NULL)
	{
		fprintf(stderr, "failed to load default font (DejaVu Sans 12): %s\n", errmsg);
		return -1;
	};
	
	return 0;
};

void gwmQuit()
{
	close(eventCounterFD);
	close(queueFD);
};

GWMWindow* gwmCreateWindow(
	GWMWindow* parent,
	const char *caption,
	unsigned int x, unsigned int y,
	unsigned int width, unsigned int height,
	int flags)
{
	uint64_t id = __sync_fetch_and_add(&nextWindowID, 1);
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.createWindow.cmd = GWM_CMD_CREATE_WINDOW;
	cmd.createWindow.id = id;
	cmd.createWindow.parent = 0;
	if (parent != NULL) cmd.createWindow.parent = parent->id;
	strcpy(cmd.createWindow.pars.caption, caption);
	strcpy(cmd.createWindow.pars.iconName, caption);
	cmd.createWindow.pars.flags = flags;
	cmd.createWindow.pars.width = width;
	cmd.createWindow.pars.height = height;
	cmd.createWindow.pars.x = x;
	cmd.createWindow.pars.y = y;
	cmd.createWindow.seq = seq;
	cmd.createWindow.painterPid = getpid();
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	if (resp.createWindowResp.status == 0)
	{
		GWMWindow *win = (GWMWindow*) malloc(sizeof(GWMWindow));
		win->id = id;
		
		uint64_t canvasBase = __alloc_pages(resp.createWindowResp.shmemSize);
		if (_glidix_shmap(canvasBase, resp.createWindowResp.shmemSize, resp.createWindowResp.shmemID, PROT_READ|PROT_WRITE) != 0)
		{
			perror("_glidix_shmap");
			free(win);
			return NULL;
		};

		win->shmemAddr = canvasBase;
		win->shmemSize = resp.createWindowResp.shmemSize;
		win->canvas = ddiCreateSurface(&resp.createWindowResp.format, resp.createWindowResp.width,
						resp.createWindowResp.height, (char*)canvasBase, DDI_STATIC_FRAMEBUFFER);
		win->handlerInfo = NULL;
		return win;
	};
	
	return NULL;
};

DDISurface* gwmGetWindowCanvas(GWMWindow *win)
{
	return win->canvas;
};

void gwmDestroyWindow(GWMWindow *win)
{
	munmap((void*)win->shmemAddr, win->shmemSize);
	GWMCommand cmd;
	cmd.destroyWindow.cmd = GWM_CMD_DESTROY_WINDOW;
	cmd.destroyWindow.id = win->id;
	
	if (_glidix_mqsend(queueFD, guiPid, guiFD, &cmd, sizeof(GWMCommand)) != 0)
	{
		perror("_glidix_mqsend");
	};
	
	if (win->handlerInfo != NULL)
	{
		GWMHandlerInfo *info = win->handlerInfo;
		if (info->prev != NULL) info->prev->next = info->next;
		if (info->next != NULL) info->next->prev = info->prev;
		if (firstHandler == info) firstHandler = info->next;
		free(info);
	};
	
	free(win);
};

void gwmPostDirty(GWMWindow *win)
{
	GWMCommand cmd;
	cmd.postDirty.cmd = GWM_CMD_POST_DIRTY;
	cmd.postDirty.id = win->id;
	_glidix_mqsend(queueFD, guiPid, guiFD, &cmd, sizeof(GWMCommand));
};

void gwmWaitEvent(GWMEvent *ev)
{
	int one = 1;
	while (ioctl(eventCounterFD, _GLIDIX_IOCTL_SEMA_WAIT, &one) == -1);

	pthread_mutex_lock(&eventLock);
	memcpy(ev, &firstEvent->payload, sizeof(GWMEvent));
	firstEvent = firstEvent->next;
	if (firstEvent != NULL) firstEvent->prev = NULL;
	pthread_mutex_unlock(&eventLock);
};

void gwmClearWindow(GWMWindow *win)
{
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.clearWindow.cmd = GWM_CMD_CLEAR_WINDOW;
	cmd.clearWindow.id = win->id;
	cmd.clearWindow.seq = seq;
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
};

void gwmPostUpdate(GWMWindow *win)
{
	EventBuffer *buf = (EventBuffer*) malloc(sizeof(EventBuffer));
	buf->payload.type = GWM_EVENT_UPDATE;
	buf->payload.win = 0;
	if (win != NULL)
	{
		buf->payload.win = win->id;
	};
	
	pthread_mutex_lock(&eventLock);
	if (firstEvent == NULL)
	{
		buf->prev = buf->next = NULL;
		firstEvent = lastEvent = buf;
	}
	else
	{
		buf->prev = lastEvent;
		buf->next = NULL;
		lastEvent->next = buf;
		lastEvent = buf;
	};
	pthread_mutex_unlock(&eventLock);
	int one = 1;
	ioctl(eventCounterFD, _GLIDIX_IOCTL_SEMA_SIGNAL, &one);
};

void gwmSetEventHandler(GWMWindow *win, GWMEventHandler handler)
{
	GWMHandlerInfo *info = (GWMHandlerInfo*) malloc(sizeof(GWMHandlerInfo));
	info->win = win;
	info->callback = handler;
	info->prev = NULL;
	info->next = firstHandler;
	if (firstHandler != NULL) firstHandler->prev = info;
	firstHandler = info;
	win->handlerInfo = info;
};

int gwmDefaultHandler(GWMEvent *ev, GWMWindow *win)
{
	return 0;
};

void gwmMainLoop()
{
	GWMEvent ev;
	GWMHandlerInfo *info;
	
	while (1)
	{
		gwmWaitEvent(&ev);
		
		for (info=firstHandler; info!=NULL; info=info->next)
		{
			if (ev.win == info->win->id)
			{
				if (info->callback(&ev, info->win) != 0)
				{
					return;
				}
				else
				{
					break;
				};
			};
		};
	};
};

void gwmScreenSize(int *width, int *height)
{
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.screenSize.cmd = GWM_CMD_SCREEN_SIZE;
	cmd.screenSize.seq = seq;
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	
	if (width != NULL) *width = resp.screenSizeResp.width;
	if (height != NULL) *height = resp.screenSizeResp.height;
};

int gwmSetWindowFlags(GWMWindow *win, int flags)
{
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.setFlags.cmd = GWM_CMD_SET_FLAGS;
	cmd.setFlags.seq = seq;
	cmd.setFlags.win = win->id;
	cmd.setFlags.flags = flags;
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	
	return resp.setFlagsResp.status;
};

int gwmSetWindowCursor(GWMWindow *win, int cursor)
{
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.setCursor.cmd = GWM_CMD_SET_CURSOR;
	cmd.setCursor.seq = seq;
	cmd.setCursor.win = win->id;
	cmd.setCursor.cursor = cursor;
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	
	return resp.setCursorResp.status;
};

int gwmSetWindowIcon(GWMWindow *win, DDISurface *icon)
{
	if ((icon->width != 16) || (icon->height != 16))
	{
		return -1;
	};
	
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.setIcon.cmd = GWM_CMD_SET_ICON;
	cmd.setIcon.seq = seq;
	cmd.setIcon.win = win->id;
	memcpy(cmd.setIcon.data, icon->data, icon->format.bpp*16*16);
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	
	return resp.setIconResp.status;
};

DDIFont *gwmGetDefaultFont()
{
	return defaultFont;
};

void gwmGetScreenFormat(DDIPixelFormat *format)
{
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.getFormat.cmd = GWM_CMD_GET_FORMAT;
	cmd.getFormat.seq = seq;
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	
	memcpy(format, &resp.getFormatResp.format, sizeof(DDIPixelFormat));
};

int gwmGetDesktopWindows(GWMGlobWinRef *focused, GWMGlobWinRef *winlist)
{
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.getWindowList.cmd = GWM_CMD_GET_WINDOW_LIST;
	cmd.getWindowList.seq = seq;
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	
	if (winlist != NULL)
	{
		memcpy(winlist, resp.getWindowListResp.wins, sizeof(GWMGlobWinRef)*resp.getWindowListResp.count);
	};
	
	if (focused != NULL)
	{
		memcpy(focused, &resp.getWindowListResp.focused, sizeof(GWMGlobWinRef));
	};
	
	return resp.getWindowListResp.count;
};

int gwmGetGlobWinParams(GWMGlobWinRef *ref, GWMWindowParams *pars)
{
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.getWindowParams.cmd = GWM_CMD_GET_WINDOW_PARAMS;
	cmd.getWindowParams.seq = seq;
	memcpy(&cmd.getWindowParams.ref, ref, sizeof(GWMGlobWinRef));
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	
	if (resp.getWindowParamsResp.status == 0)
	{
		memcpy(pars, &resp.getWindowParamsResp.params, sizeof(GWMWindowParams));
	};
	
	return resp.getWindowParamsResp.status;
};

void gwmSetListenWindow(GWMWindow *win)
{
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.setListenWindow.cmd = GWM_CMD_SET_LISTEN_WINDOW;
	cmd.setListenWindow.seq = seq;
	cmd.setListenWindow.win = win->id;
	
	_glidix_mqsend(queueFD, guiPid, guiFD, &cmd, sizeof(GWMCommand));
};
