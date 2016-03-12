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

#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <glidix/video.h>
#include <libddi.h>
#include <pthread.h>
#include <glidix/humin.h>
#include <sys/glidix.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include "gui.h"

#define	GUI_WINDOW_BORDER				2
#define	GUI_CAPTION_HEIGHT				20

DDIPixelFormat screenFormat;
DDISurface *desktopBackground;
DDISurface *screen;
DDISurface *frontBuffer;
DDISurface *mouseCursor;
DDISurface *defWinIcon;
DDISurface *winButtons;

unsigned int screenWidth, screenHeight;
int mouseX, mouseY;
pthread_spinlock_t mouseLock;

pthread_t inputThread;
pthread_t redrawThread;
pthread_t msgThread;

DDIColor winBackColor = {0xDD, 0xDD, 0xDD, 0xFF};		// window background color
DDIColor winDecoColor = {0x00, 0xAA, 0x00, 0xFF};		// window decoration color
DDIColor winUnfocColor = {0x55, 0x55, 0x55, 0xFF};		// unfocused window decoration color
DDIColor winCaptionColor = {0xFF, 0xFF, 0xFF, 0xFF};		// window caption (text) color

typedef struct Window_
{
	struct Window_*				prev;
	struct Window_*				next;
	struct Window_*				children;
	struct Window_*				parent;
	GWMWindowParams				params;
	DDISurface*				clientArea;
	DDISurface*				icon;
	uint64_t				id;
	int					pid;
	int					fd;
	uint64_t				shmemID;
	uint64_t				shmemAddr;
	uint64_t				shmemSize;
} Window;

pthread_spinlock_t windowLock;
Window* desktopWindows = NULL;

/**
 * The currently focused window. All its ancestors are considered focused too.
 */
Window* focusedWindow = NULL;

/**
 * The window that is currently being moved around by the user; NULL means the user
 * isn't moving a window. movingOffX and movingOffY define how far form the corner of
 * the window the user has clicked.
 */
Window *movingWindow = NULL;
int movingOffX;
int movingOffY;

pthread_spinlock_t redrawSignal;

/**
 * The file descriptor representing the message queue.
 */
int guiQueue = -1;

int mouseLeftDown = 0;

int isWindowFocused(Window *win)
{
	Window *check = focusedWindow;
	while (check != NULL)
	{
		if (check == win)
		{
			return 1;
		};
		
		check = check->parent;
	};
	
	return 0;
};

void PaintWindows(Window *win, DDISurface *target)
{
	for (; win!=NULL; win=win->next)
	{
		if ((win->params.flags & GWM_WINDOW_HIDDEN) == 0)
		{
			DDISurface *display = ddiCreateSurface(&target->format, win->params.width, win->params.height,
				win->clientArea->data, 0);
			PaintWindows(win->children, display);
			
			if (win->parent == NULL)
			{
				if ((win->params.flags & GWM_WINDOW_NODECORATE) == 0)
				{
					DDIColor *color = &winUnfocColor;
					if (isWindowFocused(win))
					{
						color = &winDecoColor;
					};
					
					ddiFillRect(target, win->params.x, win->params.y,
						win->params.width+2*GUI_WINDOW_BORDER,
						win->params.height+2*GUI_WINDOW_BORDER+GUI_CAPTION_HEIGHT,
						color);
					ddiBlit(win->icon, 0, 0, target, win->params.x+2, win->params.y+2, 16, 16);
					ddiDrawText(target, win->params.x+20, win->params.y+6, win->params.caption,
						&winCaptionColor, NULL);
					
					int btnIndex = ((mouseX - (int)win->params.x) - ((int)win->params.width-GUI_WINDOW_BORDER-48))/16;
					if ((mouseY < (int)win->params.y+GUI_WINDOW_BORDER) || (mouseY > (int)win->params.y+GUI_CAPTION_HEIGHT+GUI_WINDOW_BORDER))
					{
						btnIndex = -1;
					};
					
					int i;
					for (i=0; i<3; i++)
					{
						int yi = 0;
						if (i == btnIndex)
						{
							yi = 1;
							if (mouseLeftDown)
							{
								yi = 2;
							};
						};
						ddiBlit(winButtons, 16*i, 16*yi, target, win->params.x+(win->params.width-48)+16*i, win->params.y+GUI_WINDOW_BORDER, 16, 16);
					};
					
					ddiOverlay(display, 0, 0, target,
						win->params.x+GUI_WINDOW_BORDER,
						win->params.y+GUI_WINDOW_BORDER+GUI_CAPTION_HEIGHT,
						win->params.width, win->params.height);
				};
			}
			else
			{
				ddiBlit(display, 0, 0, target,
					win->params.x, win->params.y, win->params.width, win->params.height);
			};
		};
	};
};

Window *GetWindowByIDFromList(Window *win, uint64_t id)
{
	for (; win!=NULL; win=win->next)
	{
		if (win->id == id) return win;
		Window *child = GetWindowByIDFromList(win->children, id);
		if (child != NULL) return child;
	};
	
	return NULL;
};

Window *GetWindowByID(uint64_t id)
{
	return GetWindowByIDFromList(desktopWindows, id);
};

void DeleteWindow(Window *win);

void DeleteWindowList(Window *win)
{
	for (; win!=NULL; win=win->next)
	{
		DeleteWindow(win);
	};
};

void DeleteWindow(Window *win)
{
	if (focusedWindow == win) focusedWindow = NULL;
	if (movingWindow == win) movingWindow = NULL;
	
	DeleteWindowList(win->children);
	
	// unlink from previous window
	if (win->parent == NULL)
	{
		if (win->prev == NULL)
		{
			desktopWindows = win->next;
		}
		else
		{
			win->prev->next = win->next;
		};
	}
	else
	{
		if (win->prev == NULL)
		{
			win->parent->children = win->next;
		}
		else
		{
			win->prev->next = win->next;
		};
	};
	
	// unlink from next window
	if (win->next != NULL)
	{
		win->next->prev = win->prev;
	};
	
	ddiDeleteSurface(win->clientArea);
	free(win);
};

void DeleteWindowsOf(int pid, int fd)
{
	// we only need to scan the top-level windows because all windows
	// created by the application must be either top-level or children
	// of top-level windows created by it.
	Window *win;
	for (win=desktopWindows; win!=NULL; win=win->next)
	{
		if ((win->pid == pid) && (win->fd == fd))
		{
			DeleteWindow(win);
		};
	};
};

Window* CreateWindow(uint64_t parentID, GWMWindowParams *pars, uint64_t myID, int pid, int fd, int painterPid)
{
	if (myID == 0)
	{
		return NULL;
	};
	
	Window *parent = NULL;
	if (parentID != 0)
	{
		parent = GetWindowByID(parentID);
		if (parent == NULL)
		{
			return NULL;
		};
		
		if ((parent->pid != pid) && (parent->fd != fd))
		{
			return NULL;
		};
	};
	
	Window *win = (Window*) malloc(sizeof(Window));
	win->prev = NULL;
	win->next = NULL;
	win->children = NULL;
	win->parent = parent;
	memcpy(&win->params, pars, sizeof(GWMWindowParams));
	
	uint64_t memoryNeeded = ddiGetFormatDataSize(&screen->format, pars->width, pars->height);
	if (memoryNeeded & 0xFFF)
	{
		memoryNeeded &= ~0xFFF;
		memoryNeeded += 0x1000;
	};
	
	win->shmemAddr = __alloc_pages(memoryNeeded);
	win->shmemSize = memoryNeeded;
	win->shmemID = _glidix_shmalloc(win->shmemAddr, win->shmemSize, painterPid, PROT_READ|PROT_WRITE, 0);
	if (win->shmemID == 0)
	{
		printf("Shared memory allocation failed!\n");
		free(win);
		return NULL;
	};
	
	win->clientArea = ddiCreateSurface(&screen->format, pars->width, pars->height, (void*)win->shmemAddr, DDI_STATIC_FRAMEBUFFER);
	ddiFillRect(win->clientArea, 0, 0, pars->width, pars->height, &winBackColor);
	win->icon = defWinIcon;
	win->id = myID;
	win->pid = pid;
	win->fd = fd;

	if (parent == NULL)
	{
		if (desktopWindows == NULL)
		{
			desktopWindows = win;
		}
		else
		{
			Window *last = desktopWindows;
			while (last->next != NULL) last = last->next;
			last->next = win;
			win->prev = last;
		};
	}
	else
	{
		if (parent->children == NULL)
		{
			parent->children = win;
		}
		else
		{
			Window *last = parent->children;
			while (last->next != NULL) last = last->next;
			last->next = win;
			win->prev = last;
		};
	};
	return win;
};

void PaintDesktop()
{
	ddiOverlay(desktopBackground, 0, 0, screen, 0, 0, screenWidth, screenHeight);
	
	pthread_spin_lock(&windowLock);
	PaintWindows(desktopWindows, screen);
	pthread_spin_unlock(&windowLock);
	
	pthread_spin_lock(&mouseLock);
	ddiBlit(mouseCursor, 0, 0, screen, mouseX, mouseY, 16, 16);
	pthread_spin_unlock(&mouseLock);
	
	ddiOverlay(screen, 0, 0, frontBuffer, 0, 0, screenWidth, screenHeight);
};

void GetWindowSize(Window *win, int *width, int *height)
{
	if (win->parent == NULL)
	{
		if ((win->params.flags & GWM_WINDOW_NODECORATE) == 0)
		{
			*width = win->params.width + 2 * GUI_WINDOW_BORDER;
			*height = win->params.height + 2 * GUI_WINDOW_BORDER + GUI_CAPTION_HEIGHT;
			return;
		};
	};
	
	*width = win->params.width;
	*height = win->params.height;
};

void GetClientOffset(Window *win, int *offX, int *offY)
{
	if (win->parent == NULL)
	{
		if ((win->params.flags & GWM_WINDOW_NODECORATE) == 0)
		{
			*offX = GUI_WINDOW_BORDER;
			*offY = GUI_WINDOW_BORDER + GUI_CAPTION_HEIGHT;
			return;
		};
	};
	
	*offX = 0;
	*offY = 0;
};

Window* FindWindowFromListAt(Window *win, int x, int y)
{
	Window *result = NULL;
	for (; win!=NULL; win=win->next)
	{
		int width, height;
		GetWindowSize(win, &width, &height);
		
		int endX = win->params.x + width;
		int endY = win->params.y + height;
		
		int offX, offY;
		GetClientOffset(win, &offX, &offY);
		
		if ((x >= win->params.x) && (x < endX) && (y >= win->params.y) && (y < endY))
		{
			Window *child = FindWindowFromListAt(win->children, x-win->params.x-offX, y-win->params.y-offY);
			if (child == NULL) result = win;
			else result = child;
		};
	};
	
	return result;
};

Window* FindWindowAt(int x, int y)
{
	return FindWindowFromListAt(desktopWindows, x, y);
};

void onMouseLeft()
{
	mouseLeftDown = 1;
	int x, y;
	pthread_spin_lock(&mouseLock);
	x = mouseX;
	y = mouseY;
	pthread_spin_unlock(&mouseLock);
	
	pthread_spin_lock(&windowLock);
	focusedWindow = FindWindowAt(x, y);
	
	if (focusedWindow != NULL)
	{
		Window *toplevel = focusedWindow;
		while (toplevel->parent != NULL) toplevel = toplevel->parent;
		
		// bring it to front
		if (toplevel->next != NULL)
		{
			if (toplevel->prev != NULL) toplevel->prev->next = toplevel->next;
			else
			{
				desktopWindows = toplevel->next;
			};
			if (toplevel->next != NULL) toplevel->next->prev = toplevel->prev;
		
			Window *last = toplevel->next;
			while (last->next != NULL) last = last->next;
			last->next = toplevel;
			toplevel->prev = last;
			toplevel->next = NULL;
		};
		
		if ((toplevel->params.flags & GWM_WINDOW_NODECORATE) == 0)
		{
			// if the user clicked outside of this window's client area, it means
			// they clicked on the decoration. so if they clicked the title bar,
			// we allow them to move the window.
			int offX = x - toplevel->params.x;
			int offY = y - toplevel->params.y;
		
			if (offY < GUI_CAPTION_HEIGHT)
			{
				movingWindow = focusedWindow;
				movingOffX = offX;
				movingOffY = offY;
			};
		};
	};
	
	pthread_spin_unlock(&windowLock);
};

void PostWindowEvent(Window *win, GWMEvent *event)
{
	GWMMessage msg;
	msg.event.type = GWM_MSG_EVENT;
	msg.event.seq = 0;
	memcpy(&msg.event.payload, event, sizeof(GWMEvent));
	_glidix_mqsend(guiQueue, win->pid, win->fd, &msg, sizeof(GWMMessage));
};

void onMouseLeftRelease()
{
	mouseLeftDown = 0;
	
	pthread_spin_lock(&mouseLock);
	int x = mouseX;
	int y = mouseY;
	pthread_spin_unlock(&mouseLock);
	
	pthread_spin_lock(&windowLock);
	movingWindow = NULL;	
	Window *win = FindWindowAt(x, y);
	
	if (win != NULL)
	{
		if (win->parent == NULL)
		{
			if ((win->params.flags & GWM_WINDOW_NODECORATE) == 0)
			{
				int btnIndex = ((x - (int)win->params.x) - ((int)win->params.width-GUI_WINDOW_BORDER-48))/16;
				if ((y < (int)win->params.y+GUI_WINDOW_BORDER) || (y > (int)win->params.y+GUI_CAPTION_HEIGHT+GUI_WINDOW_BORDER))
				{
					btnIndex = -1;
				};
				
				if (btnIndex == 2)
				{
					// clicked the close button
					GWMEvent event;
					event.type = GWM_EVENT_CLOSE;
					event.win = win->id;
					PostWindowEvent(win, &event);
				};
			};
		};
	};
	
	pthread_spin_unlock(&windowLock);
};

void onMouseMoved()
{
	int x, y;
	pthread_spin_lock(&mouseLock);
	x = mouseX;
	y = mouseY;
	pthread_spin_unlock(&mouseLock);
	
	pthread_spin_lock(&windowLock);
	if (movingWindow != NULL)
	{
		int newX = x - movingOffX;
		int newY = y - movingOffY;
		if (newX < 0) newX = 0;
		if (newY < 0) newY = 0;
		
		movingWindow->params.x = newX;
		movingWindow->params.y = newY;
	};
	pthread_spin_unlock(&windowLock);
};

void ClipMouse()
{
	if (mouseX < 0) mouseX = 0;
	if (mouseX >= screenWidth) mouseX = screenWidth - 1;
	if (mouseY < 0) mouseY = 0;
	if (mouseY >= screenHeight) mouseY = screenHeight - 1;
};

void *inputThreadFunc(void *ignore)
{
	int fd = open("/dev/humin0", O_RDWR);
	if (fd == -1)
	{
		fprintf(stderr, "input thread cannot open device: no input\n");
		return NULL;
	};
	
	HuminEvent ev;
	while (1)
	{
		int screenDirty = 0;
		
		if (read(fd, &ev, sizeof(HuminEvent)) < 0)
		{
			continue;
		};
		
		if (ev.type == HUMIN_EV_MOTION_RELATIVE)
		{
			pthread_spin_lock(&mouseLock);
			mouseX += ev.motion.x;
			mouseY += ev.motion.y;
			ClipMouse();
			pthread_spin_unlock(&mouseLock);
			onMouseMoved();
			screenDirty = 1;
		}
		else if (ev.type == HUMIN_EV_MOTION_ABSOLUTE)
		{
			pthread_spin_lock(&mouseLock);
			mouseX = ev.motion.x;
			mouseY = ev.motion.y;
			ClipMouse();
			pthread_spin_unlock(&mouseLock);
			onMouseMoved();
			screenDirty = 1;
		}
		else if (ev.type == HUMIN_EV_BUTTON_DOWN)
		{
			if ((ev.button.scancode >= 0x100) && (ev.button.scancode < 0x200))
			{
				// mouse
				if (ev.button.scancode == HUMIN_SC_MOUSE_LEFT)
				{
					onMouseLeft();
					screenDirty = 1;
				};
			};
		}
		else if (ev.type == HUMIN_EV_BUTTON_UP)
		{
			if ((ev.button.scancode >= 0x100) && (ev.button.scancode < 0x200))
			{
				// mouse
				if (ev.button.scancode == HUMIN_SC_MOUSE_LEFT)
				{
					onMouseLeftRelease();
					screenDirty = 1;
				};
			};
		};
		
		if (screenDirty)
		{
			pthread_spin_unlock(&redrawSignal);
			pthread_kill(redrawThread, SIGCONT);
		};
	};
	
	return NULL;
};

volatile int msgReady = 0;
void *msgThreadFunc(void *ignore)
{
	static char msgbuf[65536];

	guiQueue = _glidix_mqserver();
	if (guiQueue == -1)
	{
		fprintf(stderr, "Failed to open GUI server!\n");
		return NULL;
	};

	FILE *fp = fopen("/usr/share/gui.pid", "wb");
	fprintf(fp, "%d.%d", getpid(), guiQueue);
	fclose(fp);
	msgReady = 1;
	
	while (1)
	{
		_glidix_msginfo info;
		ssize_t size = _glidix_mqrecv(guiQueue, &info, msgbuf, 65536);
		if (size == -1) continue;

		if (info.type == _GLIDIX_MQ_CONNECT)
		{
		}
		else if (info.type == _GLIDIX_MQ_INCOMING)
		{
			if (size < sizeof(GWMCommand))
			{
				continue;
			};
		
			GWMCommand *cmd = (GWMCommand*) msgbuf;
			if (cmd->cmd == GWM_CMD_CREATE_WINDOW)
			{
				pthread_spin_lock(&windowLock);
				Window * win = CreateWindow(cmd->createWindow.parent,
						&cmd->createWindow.pars, cmd->createWindow.id,
						info.pid, info.fd, cmd->createWindow.painterPid);
				
				GWMMessage msg;
				msg.createWindowResp.type = GWM_MSG_CREATE_WINDOW_RESP;
				msg.createWindowResp.seq = cmd->createWindow.seq;
				msg.createWindowResp.status = 0;
				if (win == NULL) msg.createWindowResp.status = 1;
				else
				{
					memcpy(&msg.createWindowResp.format, &screen->format, sizeof(DDIPixelFormat));
					msg.createWindowResp.shmemID = win->shmemID;
					msg.createWindowResp.shmemSize = win->shmemSize;
					msg.createWindowResp.width = win->params.width;
					msg.createWindowResp.height = win->params.height;
				};
				pthread_spin_unlock(&windowLock);
				_glidix_mqsend(guiQueue, info.pid, info.fd, &msg, sizeof(GWMMessage));
				pthread_spin_unlock(&redrawSignal);
				pthread_kill(redrawThread, SIGCONT);
			}
			else if (cmd->cmd == GWM_CMD_POST_DIRTY)
			{
				pthread_spin_unlock(&redrawSignal);
				pthread_kill(redrawThread, SIGCONT);
			};
		}
		else if (info.type == _GLIDIX_MQ_HANGUP)
		{
			pthread_spin_lock(&windowLock);
			DeleteWindowsOf(info.pid, info.fd);
			pthread_spin_unlock(&windowLock);
			pthread_kill(redrawThread, SIGCONT);
		};
	};
};

int main()
{
	int fd = open("/dev/bga", O_RDWR);
	if (fd == -1)
	{
		perror("failed to open /dev/bga");
		return 1;
	};

	LGIDisplayDeviceStat	dstat;
	LGIDisplayMode		mode;

	if (ioctl(fd, IOCTL_VIDEO_DEVSTAT, &dstat) != 0)
	{
		perror("devstat failed");
		close(fd);
		return 1;
	};

	printf("this device supports %d display modes\n", dstat.numModes);
	printf("#\tRes\n");
	for (mode.index=0; mode.index<dstat.numModes; mode.index++)
	{
		ioctl(fd, IOCTL_VIDEO_MODSTAT, &mode);
		printf("%d\t%dx%d\n", (int)mode.index, (int)mode.width, (int)mode.height);
	};

	printf("Select video mode: ");
	fflush(stdout);
	int modeSelected;
	fscanf(stdin, "%d", &modeSelected);
	mode.index = modeSelected;
	ioctl(fd, IOCTL_VIDEO_MODSTAT, &mode);

	printf("Switching to: %dx%d\n", mode.width, mode.height);
	ioctl(fd, IOCTL_VIDEO_SETMODE, &mode);

	size_t size = mode.width * mode.height * 4;

	printf("Trying to map video memory\n");
	char *videoram = (char*) mmap(NULL, size, PROT_WRITE, MAP_SHARED, fd, 0);
	if (videoram == ((char*)-1))
	{
		perror("failed to map video memory");
		close(fd);
		return 1;
	};

	screenFormat.bpp = 4;
	screenFormat.redMask = 0xFF0000;
	screenFormat.greenMask = 0x00FF00;
	screenFormat.blueMask = 0x0000FF;
	screenFormat.alphaMask = 0xFF000000;
	screenFormat.pixelSpacing = 0;
	screenFormat.scanlineSpacing = 0;
	
	screenWidth = mode.width;
	screenHeight = mode.height;
	
	frontBuffer = ddiCreateSurface(&screenFormat, mode.width, mode.height, videoram, DDI_STATIC_FRAMEBUFFER);
	screen = ddiCreateSurface(&screenFormat, mode.width, mode.height, NULL, 0);
	
	DDIColor white = {0xFF, 0xFF, 0xFF, 0xFF};
	ddiDrawText(frontBuffer, 5, 5, "GUI loading, please wait...", &white, NULL);

	DDIColor backgroundColor = {0x20, 0x20, 0xF0, 0xFF};
	const char *errmsg;
	desktopBackground = ddiLoadAndConvertPNG(&screenFormat, "/usr/share/images/wallpaper.png", &errmsg);
	if (desktopBackground == NULL)
	{
		printf("Failed to load wallpaper: %s\n", errmsg);
		desktopBackground = ddiCreateSurface(&screenFormat, mode.width, mode.height, NULL, 0);
		ddiFillRect(desktopBackground, 0, 0, mode.width, mode.height, &backgroundColor);
	};
	
	// initialize mouse cursor
	DDIColor mouseColor = {0xEE, 0xEE, 0xEE, 0xFF};
	
	mouseCursor = ddiLoadAndConvertPNG(&screenFormat, "/usr/share/images/cursor.png", NULL);
	if (mouseCursor == NULL)
	{
		mouseCursor = ddiCreateSurface(&screenFormat, 16, 16, NULL, 0);
		ddiFillRect(mouseCursor, 0, 0, 16, 16, &mouseColor);
	};

	mouseX = mode.width / 2 - 8;
	mouseY = mode.height / 2 - 8;
	mouseX = 205;
	mouseY = 15;
	
	// system images
	defWinIcon = ddiLoadAndConvertPNG(&screenFormat, "/usr/share/images/defwinicon.png", NULL);
	if (defWinIcon == NULL)
	{
		defWinIcon = ddiCreateSurface(&screenFormat, 16, 16, NULL, 0);
		ddiFillRect(defWinIcon, 0, 0, 16, 16, &mouseColor);
	};
	
	winButtons = ddiLoadAndConvertPNG(&screenFormat, "/usr/share/images/winbuttons.png", NULL);
	if (winButtons == NULL)
	{
		winButtons = ddiCreateSurface(&screenFormat, 48, 64, NULL, 0);
		ddiFillRect(winButtons, 0, 0, 48, 64, &mouseColor);
	};

#if 0
	GWMWindowParams testpars;
	strcpy(testpars.caption, "Example Window");
	strcpy(testpars.iconName, "Example Window");
	testpars.flags = 0;
	testpars.width = 200;
	testpars.height = 200;
	testpars.x = 100;
	testpars.y = 100;
	Window *win = CreateWindow(NULL, &testpars);
	ddiDrawText(win->clientArea, 5, 5, "Hello, world!", NULL, NULL);
	
	GWMWindowParams pars2;
	strcpy(pars2.caption, "Another window");
	strcpy(pars2.iconName, "Another window");
	pars2.flags = 0;
	pars2.width = 300;
	pars2.height = 150;
	pars2.x = 500;
	pars2.y = 500;
	Window *win2 = CreateWindow(NULL, &pars2);
#endif

	unlink("/usr/share/gui.pid");
	pthread_spin_init(&mouseLock);
	if (pthread_create(&inputThread, NULL, inputThreadFunc, NULL) != 0)
	{
		fprintf(stderr, "failed to create input thread!\n");
		return 1;
	};

	if (pthread_create(&msgThread, NULL, msgThreadFunc, NULL) != 0)
	{
		fprintf(stderr, "failed to create server thread!\n");
		return 1;
	};
	
	while (!msgReady) __sync_synchronize();
	
	pid_t child = fork();
	if (child == -1)
	{
		perror("fork");
		return 1;
	};
	
	if (child == 0)
	{
		execl("/usr/libexec/gui-init", "gui-init", NULL);
		return 1;
	}
	else
	{
		waitpid(child, NULL, WNOHANG | WDETACH);
	};

	redrawThread = pthread_self();
	uint8_t ignoreByte;
	while (1)
	{
		PaintDesktop();
		while (_glidix_condwait(&redrawSignal, &ignoreByte) != 0);
	};
	
	return 0;
};
