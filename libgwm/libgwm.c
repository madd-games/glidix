/*
	Glidix GUI

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <libgwm.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>

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
	sem_t				lock;
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
static GWMInfo *gwminfo = NULL;

DDIColor gwmColorSelection = {0, 0xAA, 0, 0xFF};

static void gwmPostWaiter(uint64_t seq, GWMMessage *resp, const GWMCommand *cmd)
{
	GWMWaiter *waiter = (GWMWaiter*) malloc(sizeof(GWMWaiter));
	waiter->prev = NULL;
	waiter->next = NULL;
	waiter->seq = seq;
	sem_init(&waiter->lock, 0, 0);
	
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
	sem_wait(&waiter->lock);
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
						sem_post(&waiter->lock);
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
	int fd = open("/run/gwminfo", O_RDONLY);
	if (fd == -1)
	{
		return -1;
	};
	
	gwminfo = (GWMInfo*) mmap(NULL, sizeof(GWMInfo), PROT_READ, MAP_SHARED, fd, 0);
	guiPid = gwminfo->pid;
	guiFD = gwminfo->fd;
	
	nextWindowID = 1;
	nextSeq = 1;
	eventCounterFD = _glidix_thsync(1, 0);	// semaphore, start at zero
	
	pthread_mutex_init(&waiterLock, NULL);
	pthread_mutex_init(&eventLock, NULL);
	
	char dispdev[1024];
	char linebuf[1024];
	dispdev[0] = 0;
	
	FILE *fp = fopen("/etc/gwm.conf", "r");
	if (fp == NULL)
	{
		fprintf(stderr, "gwm: failed to open configuration file /etc/gwm.conf: %s\n", strerror(errno));
		return -1;
	};
	
	char *saveptr;
	char *line;
	int lineno = 0;
	while ((line = fgets(linebuf, 1024, fp)) != NULL)
	{
		lineno++;
		
		char *endline = strchr(line, '\n');
		if (endline != NULL)
		{
			*endline = 0;
		};
		
		if (strlen(line) >= 1023)
		{
			fprintf(stderr, "/etc/gwm.conf:%d: buffer overflow\n", lineno);
			fclose(fp);
			return -1;
		};
		
		if ((line[0] == 0) || (line[0] == '#'))
		{
			continue;
		}
		else
		{
			char *cmd = strtok_r(line, " \t", &saveptr);
			if (cmd == NULL)
			{
				continue;
			};
			
			if (strcmp(cmd, "display") == 0)
			{
				char *name = strtok_r(NULL, " \t", &saveptr);
				if (name == NULL)
				{
					fprintf(stderr, "/etc/gwm.conf:%d: 'display' needs a parameter\n", lineno);
					fclose(fp);
					return -1;
				};
				
				strcpy(dispdev, name);
			};
		};
	};
	fclose(fp);
	
	if (dispdev[0] == 0)
	{
		fprintf(stderr, "gwm: display device not found\n");
		return -1;
	};
	
	if (ddiInit(dispdev, O_RDONLY) != 0)
	{
		fprintf(stderr, "GWM: failed to initialize DDI: %s\n", strerror(errno));
		return -1;
	};

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
	ddiQuit();
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

		win->canvases[0] = ddiOpenSurface(resp.createWindowResp.clientID[0]);
		win->canvases[1] = ddiOpenSurface(resp.createWindowResp.clientID[1]);
		
		win->handlerInfo = NULL;
		win->currentBuffer = 1;
		win->lastClickTime = 0;
		
		win->modalID = 0;
		if (parent != NULL) win->modalID = parent->modalID;
		return win;
	};
	
	return NULL;
};

DDISurface* gwmGetWindowCanvas(GWMWindow *win)
{
	return win->canvases[win->currentBuffer];
};

void gwmDestroyWindow(GWMWindow *win)
{
	ddiDeleteSurface(win->canvases[0]);
	ddiDeleteSurface(win->canvases[1]);
	
	GWMCommand cmd;
	cmd.destroyWindow.cmd = GWM_CMD_DESTROY_WINDOW;
	cmd.destroyWindow.id = win->id;
	
	if (_glidix_mqsend(queueFD, guiPid, guiFD, &cmd, sizeof(GWMCommand)) != 0)
	{
		perror("_glidix_mqsend");
		return;
	};
	
	if (win->handlerInfo != NULL)
	{
		GWMHandlerInfo *info = win->handlerInfo;
		if (info->prev != NULL) info->prev->next = info->next;
		if (info->next != NULL) info->next->prev = info->prev;
		if (firstHandler == info) firstHandler = info->next;
		
		if (info->state == 0)
		{
			free(info);
		}
		else
		{
			info->state = 2;
		};
	};
	
	free(win);
};

void gwmPostDirty(GWMWindow *win)
{
	//printf("gwmPostDirty\n");
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.postDirty.cmd = GWM_CMD_POST_DIRTY;
	cmd.postDirty.id = win->id;
	cmd.postDirty.seq = seq;
	_glidix_mqsend(queueFD, guiPid, guiFD, &cmd, sizeof(GWMCommand));

	//GWMMessage resp;
	//gwmPostWaiter(seq, &resp, &cmd);
	
	win->currentBuffer ^= 1;
	//printf("buffer: %d\n", win->currentBuffer);
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
	info->state = 0;
	if (firstHandler != NULL) firstHandler->prev = info;
	firstHandler = info;
	win->handlerInfo = info;
};

int gwmDefaultHandler(GWMEvent *ev, GWMWindow *win)
{
	switch (ev->type)
	{
	case GWM_EVENT_CLOSE:
		return -1;
	default:
		return 0;
	};
};

static void gwmModalLoop(uint64_t modalID)
{
	GWMEvent ev;
	GWMHandlerInfo *info;
	
	while (1)
	{
		gwmWaitEvent(&ev);
		
		for (info=firstHandler; info!=NULL; info=info->next)
		{
			if ((ev.win == info->win->id) && (info->win->modalID == modalID))
			{
				info->state = 1;
				if (info->callback(&ev, info->win) != 0)
				{
					return;
				}
				else
				{
					if (info->state == 1)
					{
						if ((ev.type == GWM_EVENT_UP) && (ev.scancode == GWM_SC_MOUSE_LEFT))
						{
							clock_t now = clock();
							if (info->win->lastClickTime != 0)
							{
								if ((now-info->win->lastClickTime) <= GWM_DOUBLECLICK_TIMEOUT)
								{
									if ((info->win->lastClickX == ev.x)
										&& (info->win->lastClickY == ev.y))
									{
										ev.type = GWM_EVENT_DOUBLECLICK;
										if (info->callback(&ev, info->win) != 0)
										{
											return;
										};
									};
								};
							};
						
							info->win->lastClickX = ev.x;
							info->win->lastClickY = ev.y;
							info->win->lastClickTime = now;
						};
					
						info->state = 0;
					}
					else
					{
						free(info);
					};
					break;
				};
			};
		};
	};
};

void gwmMainLoop()
{
	gwmModalLoop(0);
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

void gwmToggleWindow(GWMGlobWinRef *ref)
{
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.toggleWindow.cmd = GWM_CMD_TOGGLE_WINDOW;
	cmd.toggleWindow.seq = seq;
	memcpy(&cmd.toggleWindow.ref, ref, sizeof(GWMGlobWinRef));
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
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

void gwmResizeWindow(GWMWindow *win, unsigned int width, unsigned int height)
{
	DDIPixelFormat format;
	memcpy(&format, &win->canvases[0]->format, sizeof(DDIPixelFormat));
	
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.resize.cmd = GWM_CMD_RESIZE;
	cmd.resize.seq = seq;
	cmd.resize.win = win->id;
	cmd.resize.width = width;
	cmd.resize.height = height;
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	
	if (resp.resizeResp.status == 0)
	{
		ddiDeleteSurface(win->canvases[0]);
		ddiDeleteSurface(win->canvases[1]);
		
		win->canvases[0] = ddiOpenSurface(resp.resizeResp.clientID[0]);
		win->canvases[1] = ddiOpenSurface(resp.resizeResp.clientID[1]);
		
		win->currentBuffer = 1;
	}
	else
	{
		abort();
	};
};

void gwmMoveWindow(GWMWindow *win, int x, int y)
{
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.move.cmd = GWM_CMD_MOVE;
	cmd.move.seq = seq;
	cmd.move.win = win->id;
	cmd.move.x = x;
	cmd.move.y = y;
	
	_glidix_mqsend(queueFD, guiPid, guiFD, &cmd, sizeof(GWMCommand));
};

void gwmRelToAbs(GWMWindow *win, int relX, int relY, int *absX, int *absY)
{
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.relToAbs.cmd = GWM_CMD_REL_TO_ABS;
	cmd.relToAbs.seq = seq;
	cmd.relToAbs.win = win->id;
	cmd.relToAbs.relX = relX;
	cmd.relToAbs.relY = relY;
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	
	if (absX != NULL) *absX = resp.relToAbsResp.absX;
	if (absY != NULL) *absY = resp.relToAbsResp.absY;
};

int gwmClassifyChar(char c)
{
	if ((c == ' ') || (c == '\t') || (c == '\n'))
	{
		return 0;
	};
	
	return 1;
};

GWMWindow* gwmCreateModal(const char *caption, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	static uint64_t nextModalID = 1;
	GWMWindow *win = gwmCreateWindow(NULL, caption, x, y, width, height, GWM_WINDOW_NOTASKBAR | GWM_WINDOW_HIDDEN);
	win->modalID = nextModalID++;
	return win;
};

void gwmRunModal(GWMWindow *modal, int flags)
{
	gwmSetWindowFlags(modal, flags);
	gwmModalLoop(modal->modalID);
};

GWMInfo *gwmGetInfo()
{
	return gwminfo;
};

void gwmRedrawScreen()
{
	GWMCommand cmd;
	cmd.cmd = GWM_CMD_REDRAW_SCREEN;
	
	_glidix_mqsend(queueFD, guiPid, guiFD, &cmd, sizeof(GWMCommand));
};

void gwmGetGlobRef(GWMWindow *win, GWMGlobWinRef *ref)
{
	ref->id = win->id;
	ref->fd = queueFD;
	ref->pid = getpid();
};

GWMWindow *gwmScreenshotWindow(GWMGlobWinRef *ref)
{
	uint64_t id = __sync_fetch_and_add(&nextWindowID, 1);
	uint64_t seq = __sync_fetch_and_add(&nextSeq, 1);
	
	GWMCommand cmd;
	cmd.screenshotWindow.cmd = GWM_CMD_SCREENSHOT_WINDOW;
	cmd.screenshotWindow.id = id;
	cmd.screenshotWindow.seq = seq;
	memcpy(&cmd.screenshotWindow.ref, ref, sizeof(GWMGlobWinRef));
	
	GWMMessage resp;
	gwmPostWaiter(seq, &resp, &cmd);
	if (resp.screenshotWindowResp.status == 0)
	{
		GWMWindow *win = (GWMWindow*) malloc(sizeof(GWMWindow));
		win->id = id;

		win->canvases[0] = ddiOpenSurface(resp.screenshotWindowResp.clientID[0]);
		win->canvases[1] = ddiOpenSurface(resp.screenshotWindowResp.clientID[1]);
		
		win->handlerInfo = NULL;
		win->currentBuffer = 0;
		win->lastClickTime = 0;
		
		win->modalID = 0;
		return win;
	};
	
	return NULL;
};
