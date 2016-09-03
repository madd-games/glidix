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
#include <semaphore.h>
#include <time.h>
#include <errno.h>
#include "gui.h"

#define	GUI_WINDOW_BORDER				2
#define	GUI_CAPTION_HEIGHT				20

// US keyboard layout
static int keymap[128] =
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
		GWM_KC_UP,	/* Up Arrow */
		0,	/* Page Up */
	'-',
		GWM_KC_LEFT,	/* Left Arrow */
		0,
		GWM_KC_RIGHT,	/* Right Arrow */
	'+',
		0,	/* 79 - End key*/
		GWM_KC_DOWN,	/* Down Arrow */
		0,	/* Page Down */
		0,	/* Insert Key */
		0,	/* Delete Key */
		0,	 0,	 0,
		0,	/* F11 Key */
		0,	/* F12 Key */
		0,	/* All other keys are undefined */
};

// when shift is pressed
static int keymapShift[128] =
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

DDIPixelFormat screenFormat;
DDISurface *desktopBackground;
DDISurface *screen;
DDISurface *frontBuffer;
DDISurface *defWinIcon;
DDISurface *winButtons;
DDISurface *winCap;

unsigned int screenWidth, screenHeight;
int mouseX, mouseY;
pthread_mutex_t mouseLock;

pthread_t inputThread;
pthread_t redrawThread;
pthread_t msgThread;

DDIColor winBackColor = {0xDD, 0xDD, 0xDD, 0xFF};		// window background color
DDIColor winDecoColor = {0x00, 0xAA, 0x00, 0xB2};		// window decoration color
DDIColor winUnfocColor = {0x55, 0x55, 0x55, 0xB2};		// unfocused window decoration color
DDIColor winCaptionColor = {0xFF, 0xFF, 0xFF, 0xFF};		// window caption (text) color

DDISurface *winDeco;
DDISurface *winUnfoc;

DDIFont *captionFont;

typedef struct Window_
{
	struct Window_*				prev;
	struct Window_*				next;
	struct Window_*				children;
	struct Window_*				parent;
	GWMWindowParams				params;
	DDISurface*				clientArea;
	DDISurface*				icon;
	DDISurface*				titleBar;
	DDISurface*				display;
	int					titleBarDirty;
	int					displayDirty;
	uint64_t				id;
	int					pid;
	int					fd;
	uint64_t				shmemID;
	uint64_t				shmemAddr;
	uint64_t				shmemSize;
	int					cursor;
} Window;

pthread_mutex_t windowLock;
Window* desktopWindows = NULL;

typedef struct
{
	DDISurface*				image;
	int					hotX, hotY;
	const char*				src;
} Cursor;

static Cursor cursors[GWM_CURSOR_COUNT] = {
	{NULL, 0, 0, "/usr/share/images/cursor.png"},
	{NULL, 7, 7, "/usr/share/images/txtcursor.png"}
};

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

/**
 * The window that the mouse is currently inside of. This is used to issues enter/motion/leave
 * events.
 */
Window *hoveringWindow = NULL;

/**
 * The "listening window", to which an event is reported when the desktop is updated. This is usually
 * the system bar.
 */
Window *listenWindow = NULL;

/**
 * The semaphore counting the number of redraws to be made.
 */
sem_t semRedraw;

/**
 * The file descriptor representing the message queue.
 */
int guiQueue = -1;

int mouseLeftDown = 0;

void PostDesktopUpdate();

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
			DDISurface *display = win->display;
			if (win->displayDirty)
			{
				ddiOverlay(win->clientArea, 0, 0, display, 0, 0, win->params.width, win->params.height);
				PaintWindows(win->children, display);
				win->displayDirty = 0;
			};
			
			if (win->parent == NULL)
			{
				if ((win->params.flags & GWM_WINDOW_NODECORATE) == 0)
				{
					DDISurface *borderSurface = winUnfoc;
					if (isWindowFocused(win))
					{
						borderSurface = winDeco;
					};
					
					ddiBlit(borderSurface, 0, 0, target, win->params.x, win->params.y+GUI_CAPTION_HEIGHT,
						win->params.width+2*GUI_WINDOW_BORDER,
						win->params.height+2*GUI_WINDOW_BORDER);

					// the caption graphic
					int yoff = 0;
					if (!isWindowFocused(win))
					{
						yoff = GUI_CAPTION_HEIGHT;
					};

					ddiBlit(winCap, 0, yoff, target, win->params.x, win->params.y,
						winCap->width/2, GUI_CAPTION_HEIGHT);
					int end = win->params.width + 2*GUI_WINDOW_BORDER - winCap->width/2;
					ddiBlit(winCap, winCap->width/2+1, yoff, target, win->params.x+end, win->params.y,
						winCap->width/2, GUI_CAPTION_HEIGHT);
					
					int xoff;
					for (xoff=winCap->width/2; xoff<end; xoff++)
					{
						ddiBlit(winCap, winCap->width/2, yoff, target, win->params.x+xoff, win->params.y,
							1, GUI_CAPTION_HEIGHT);
					};

					if (win->titleBarDirty)
					{
						win->titleBarDirty = 0;
						DDIColor col = {0, 0, 0, 0};
						ddiFillRect(win->titleBar, 0, 0, win->params.width+2*GUI_WINDOW_BORDER,
							GUI_CAPTION_HEIGHT, &col);

						const char *penError;
						DDIPen *pen = ddiCreatePen(&screenFormat, captionFont, 20, 3,
							win->params.width+2*GUI_WINDOW_BORDER, GUI_CAPTION_HEIGHT,
							0, 0, &penError);
						if (pen == NULL)
						{
							printf("cannot create pen: %s\n", penError);
						}
						else
						{
							//ddiSetPenFont(pen, "DejaVu Sans", 12, DDI_STYLE_BOLD, NULL);
							ddiSetPenColor(pen, &winCaptionColor);
							ddiWritePen(pen, win->params.caption);
							ddiExecutePen(pen, win->titleBar);
							ddiDeletePen(pen);
						};
					};
					
					ddiBlit(win->titleBar, 0, 0, target, win->params.x, win->params.y,
						win->params.width+2*GUI_WINDOW_BORDER, GUI_CAPTION_HEIGHT);

					if (win->icon != NULL)
					{
						ddiBlit(win->icon, 0, 0, target, win->params.x+2, win->params.y+2, 16, 16);
					}
					else
					{
						ddiBlit(defWinIcon, 0, 0, target, win->params.x+2, win->params.y+2, 16, 16);
					};

					int btnIndex = ((mouseX - (int)win->params.x) - ((int)win->params.width-GUI_WINDOW_BORDER-48))/16;
					if ((mouseY < (int)win->params.y+GUI_WINDOW_BORDER) || (mouseY > (int)win->params.y+GUI_CAPTION_HEIGHT+GUI_WINDOW_BORDER))
					{
						btnIndex = -1;
					};
					
					if ((win->params.flags & GWM_WINDOW_NOSYSMENU) == 0)
					{
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
					};
					
					ddiOverlay(display, 0, 0, target,
						win->params.x+GUI_WINDOW_BORDER,
						win->params.y+GUI_WINDOW_BORDER+GUI_CAPTION_HEIGHT,
						win->params.width, win->params.height);
				}
				else
				{
					ddiBlit(display, 0, 0, target,
						win->params.x, win->params.y, win->params.width, win->params.height);
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

Window *GetWindowByIDFromList(Window *win, uint64_t id, int pid, int fd)
{
	for (; win!=NULL; win=win->next)
	{
		if ((win->id == id) && (win->pid == pid) && (win->fd == fd)) return win;
		Window *child = GetWindowByIDFromList(win->children, id, pid, fd);
		if (child != NULL) return child;
	};
	
	return NULL;
};

Window *GetWindowByID(uint64_t id, int pid, int fd)
{
	return GetWindowByIDFromList(desktopWindows, id, pid, fd);
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
	if (hoveringWindow == win) hoveringWindow = NULL;
	if (listenWindow == win) listenWindow = NULL;
	
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
	
	if (win->icon != NULL) ddiDeleteSurface(win->icon);
	ddiDeleteSurface(win->clientArea);
	ddiDeleteSurface(win->titleBar);
	ddiDeleteSurface(win->display);
	free(win);
	
	PostDesktopUpdate();
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

void DeleteWindowByID(uint64_t id, int pid, int fd)
{
	Window *win = GetWindowByID(id, pid, fd);
	if (win != NULL) DeleteWindow(win);
};

Window* CreateWindow(uint64_t parentID, GWMWindowParams *pars, uint64_t myID, int pid, int fd, int painterPid)
{
	PostDesktopUpdate();
	pars->caption[255] = 0;
	pars->iconName[255] = 0;
	if (myID == 0)
	{
		return NULL;
	};
	
	Window *parent = NULL;
	if (parentID != 0)
	{
		parent = GetWindowByID(parentID, pid, fd);
		if (parent == NULL)
		{
			return NULL;
		};
	};
	
	Window *win = (Window*) malloc(sizeof(Window));
	win->prev = NULL;
	win->next = NULL;
	win->children = NULL;
	win->parent = parent;
	win->cursor = GWM_CURSOR_NORMAL;
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
	win->display = ddiCreateSurface(&screen->format, pars->width, pars->height, NULL, 0);
	ddiFillRect(win->clientArea, 0, 0, pars->width, pars->height, &winBackColor);
	win->titleBar = ddiCreateSurface(&screen->format, pars->width+2*GUI_WINDOW_BORDER, GUI_CAPTION_HEIGHT, NULL, 0);
	win->titleBarDirty = 1;
	win->icon = NULL;
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
	
	pthread_mutex_lock(&windowLock);
	PaintWindows(desktopWindows, screen);
	pthread_mutex_unlock(&windowLock);

	pthread_mutex_lock(&mouseLock);
	int cursorIndex = 0;
	if (hoveringWindow != NULL)
	{
		cursorIndex = hoveringWindow->cursor;
	};
	ddiBlit(cursors[cursorIndex].image, 0, 0, screen, mouseX-cursors[cursorIndex].hotX, mouseY-cursors[cursorIndex].hotY, 16, 16);
	pthread_mutex_unlock(&mouseLock);
	
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
		if ((win->params.flags & GWM_WINDOW_HIDDEN) == 0)
		{
			int width, height;
			GetWindowSize(win, &width, &height);
		
			int endX = win->params.x + width;
			int endY = win->params.y + height;
		
			int offX, offY;
			GetClientOffset(win, &offX, &offY);
		
			if ((x >= (int)win->params.x) && (x < endX) && (y >= (int)win->params.y) && (y < endY))
			{
				Window *child = FindWindowFromListAt(win->children, x-win->params.x-offX, y-win->params.y-offY);
				if (child == NULL) return win;
				else result = child;
			};
		};
	};
	
	return result;
};

Window* FindWindowAt(int x, int y)
{
	return FindWindowFromListAt(desktopWindows, x, y);
};

void AbsoluteToRelativeCoords(Window *win, int inX, int inY, int *outX, int *outY)
{
	int offX, offY;
	GetClientOffset(win, &offX, &offY);
	
	int cornerX = win->params.x + offX;
	int cornerY = win->params.y + offY;
	
	int effectiveX = inX - cornerX;
	int effectiveY = inY - cornerY;
	
	if (win->parent == NULL)
	{
		*outX = effectiveX;
		*outY = effectiveY;
	}
	else
	{
		AbsoluteToRelativeCoords(win->parent, effectiveX, effectiveY, outX, outY);
	};
};

void PostWindowEvent(Window *win, GWMEvent *event)
{
	GWMMessage msg;
	msg.event.type = GWM_MSG_EVENT;
	msg.event.seq = 0;
	memcpy(&msg.event.payload, event, sizeof(GWMEvent));
	_glidix_mqsend(guiQueue, win->pid, win->fd, &msg, sizeof(GWMMessage));
};

void PostDesktopUpdate()
{
	GWMEvent ev;
	ev.type = GWM_EVENT_DESKTOP_UPDATE;
	if (listenWindow != NULL)
	{
		printf("sending update event to listen window\n");
		PostWindowEvent(listenWindow, &ev);
	};
};

void onMouseLeft()
{
	mouseLeftDown = 1;
	int x, y;
	pthread_mutex_lock(&mouseLock);
	x = mouseX;
	y = mouseY;
	pthread_mutex_unlock(&mouseLock);
	
	pthread_mutex_lock(&windowLock);
	Window *oldFocus = focusedWindow;
	focusedWindow = FindWindowAt(x, y);
	
	if (oldFocus != focusedWindow)
	{
		PostDesktopUpdate();
		if (oldFocus != NULL)
		{
			GWMEvent ev;
			ev.type = GWM_EVENT_FOCUS_OUT;
			ev.win = oldFocus->id;
			PostWindowEvent(oldFocus, &ev);
		};
		
		if (focusedWindow != NULL)
		{
			GWMEvent ev;
			ev.type = GWM_EVENT_FOCUS_IN;
			ev.win = focusedWindow->id;
			PostWindowEvent(focusedWindow, &ev);
		};
	};
	
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
				if ((focusedWindow->params.flags & GWM_WINDOW_NODECORATE) == 0)
				{
					movingWindow = focusedWindow;
					while (movingWindow->parent != NULL) movingWindow = movingWindow->parent;
					movingOffX = offX;
					movingOffY = offY;
				};
			};
		};
	};
	
	pthread_mutex_unlock(&windowLock);
};

void onMouseLeftRelease()
{
	mouseLeftDown = 0;
	
	pthread_mutex_lock(&mouseLock);
	int x = mouseX;
	int y = mouseY;
	pthread_mutex_unlock(&mouseLock);
	
	pthread_mutex_lock(&windowLock);
	movingWindow = NULL;	
	Window *win = FindWindowAt(x, y);
	
	if (win != NULL)
	{
		if (win->parent == NULL)
		{
			if ((win->params.flags & (GWM_WINDOW_NODECORATE | GWM_WINDOW_NOSYSMENU)) == 0)
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
	
	pthread_mutex_unlock(&windowLock);
};

static int currentKeyMods = 0;
void DecodeScancode(GWMEvent *ev)
{
	if (ev->scancode < 0x80) ev->keycode = keymap[ev->scancode];
	else ev->keycode = 0;
	ev->keychar = 0;
	if (ev->scancode == 0x1D)
	{
		// ctrl
		if (ev->type == GWM_EVENT_DOWN)
		{
			currentKeyMods |= GWM_KM_CTRL;
		}
		else
		{
			currentKeyMods &= ~GWM_KM_CTRL;
		};
	};
	
	if (ev->scancode < 0x80)
	{
		int key = keymap[ev->scancode];
		if (currentKeyMods & GWM_KM_SHIFT) key = keymapShift[ev->scancode];
		if (key == 0x80)
		{
			// shift
			if (ev->type == GWM_EVENT_DOWN)
			{
				currentKeyMods |= GWM_KM_SHIFT;
			}
			else
			{
				currentKeyMods &= ~GWM_KM_SHIFT;
			};
		}
		else if ((key != 0) && ((currentKeyMods & GWM_KM_CTRL) == 0) && (key < 0x80))
		{
			ev->keychar = key;
		};
	};
	
	ev->keymod = currentKeyMods;
};

void onInputEvent(int ev, int scancode)
{
	pthread_mutex_lock(&windowLock);
	if (focusedWindow != NULL)
	{
		GWMEvent event;
		if (ev == HUMIN_EV_BUTTON_DOWN)
		{
			event.type = GWM_EVENT_DOWN;
		}
		else
		{
			event.type = GWM_EVENT_UP;
		};
		
		event.win = focusedWindow->id;
		event.scancode = scancode;
		
		pthread_mutex_lock(&mouseLock);
		int mx = mouseX;
		int my = mouseY;
		pthread_mutex_unlock(&mouseLock);
		
		AbsoluteToRelativeCoords(focusedWindow, mx, my, &event.x, &event.y);
		DecodeScancode(&event);
		PostWindowEvent(focusedWindow, &event);
	};
	
	pthread_mutex_unlock(&windowLock);
};

void onMouseMoved()
{
	//clock_t start = clock();
	int x, y;
	pthread_mutex_lock(&mouseLock);
	x = mouseX;
	y = mouseY;
	pthread_mutex_unlock(&mouseLock);
	
	pthread_mutex_lock(&windowLock);
	if (movingWindow != NULL)
	{
		int newX = x - movingOffX;
		int newY = y - movingOffY;
		if (newX < 0) newX = 0;
		if (newY < 0) newY = 0;
		
		movingWindow->params.x = newX;
		movingWindow->params.y = newY;
	};
	
	Window *win = FindWindowAt(x, y);
	if (win != hoveringWindow)
	{
		if (hoveringWindow != NULL)
		{
			GWMEvent ev;
			ev.type = GWM_EVENT_LEAVE;
			ev.win = hoveringWindow->id;
			PostWindowEvent(hoveringWindow, &ev);
		};
		
		if (win != NULL)
		{
			GWMEvent ev;
			ev.type = GWM_EVENT_ENTER;
			ev.win = win->id;
			AbsoluteToRelativeCoords(win, x, y, &ev.x, &ev.y);
			PostWindowEvent(win, &ev);
		};
		
		hoveringWindow = win;
	}
	else if (hoveringWindow != NULL)
	{
		GWMEvent ev;
		ev.type = GWM_EVENT_MOTION;
		ev.win = hoveringWindow->id;
		AbsoluteToRelativeCoords(hoveringWindow, x, y, &ev.x, &ev.y);
		PostWindowEvent(win, &ev);
	};
	
	pthread_mutex_unlock(&windowLock);
	//clock_t end = clock();
	//printf("mouse ticks: %d\n", (int)(end-start));
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
			if (pthread_mutex_trylock(&mouseLock) != 0)
			{
				// too much contention
				continue;
			};
			mouseX += ev.motion.x;
			mouseY += ev.motion.y;
			ClipMouse();
			pthread_mutex_unlock(&mouseLock);
			onMouseMoved();
			screenDirty = 1;
		}
		else if (ev.type == HUMIN_EV_MOTION_ABSOLUTE)
		{
			pthread_mutex_lock(&mouseLock);
			mouseX = ev.motion.x;
			mouseY = ev.motion.y;
			ClipMouse();
			pthread_mutex_unlock(&mouseLock);
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
			
			onInputEvent(HUMIN_EV_BUTTON_DOWN, ev.button.scancode);
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
			
			onInputEvent(HUMIN_EV_BUTTON_UP, ev.button.scancode);
		};
		
		if (screenDirty)
		{
			sem_post(&semRedraw);
		};
	};
	
	return NULL;
};

void PostWindowDirty(Window *win)
{
	// only bother if we don't already know
	if (!win->displayDirty)
	{
		win->displayDirty = 1;
		if ((win->params.flags & GWM_WINDOW_HIDDEN) == 0)
		{
			if (win->parent == NULL)
			{
				sem_post(&semRedraw);
			}
			else
			{
				PostWindowDirty(win->parent);
			};
		};
	};
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
	
	FILE *fp = fopen("/run/gui.pid", "wb");
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
				pthread_mutex_lock(&windowLock);
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
					
					if (cmd->createWindow.pars.flags & GWM_WINDOW_MKFOCUSED)
					{
						movingWindow = NULL;
						focusedWindow = win;
					};
				};
				pthread_mutex_unlock(&windowLock);
				_glidix_mqsend(guiQueue, info.pid, info.fd, &msg, sizeof(GWMMessage));
				sem_post(&semRedraw);
			}
			else if (cmd->cmd == GWM_CMD_POST_DIRTY)
			{
				//sem_post(&semRedraw);
				pthread_mutex_lock(&windowLock);
				Window *win = GetWindowByID(cmd->postDirty.id, info.pid, info.fd);
				if (win != NULL) PostWindowDirty(win);
				pthread_mutex_unlock(&windowLock);
			}
			else if (cmd->cmd == GWM_CMD_DESTROY_WINDOW)
			{
				pthread_mutex_lock(&windowLock);
				DeleteWindowByID(cmd->destroyWindow.id, info.pid, info.fd);
				pthread_mutex_unlock(&windowLock);
				sem_post(&semRedraw);
			}
			else if (cmd->cmd == GWM_CMD_CLEAR_WINDOW)
			{
				pthread_mutex_lock(&windowLock);
				Window *win = GetWindowByID(cmd->clearWindow.id, info.pid, info.fd);
				if (win != NULL)
				{
					ddiFillRect(win->clientArea, 0, 0, win->params.width, win->params.height, &winBackColor);
				};
				
				GWMMessage msg;
				msg.clearWindowResp.type = GWM_MSG_CLEAR_WINDOW_RESP;
				msg.clearWindowResp.seq = cmd->clearWindow.seq;
				pthread_mutex_unlock(&windowLock);
				_glidix_mqsend(guiQueue, info.pid, info.fd, &msg, sizeof(GWMMessage));
			}
			else if (cmd->cmd == GWM_CMD_SCREEN_SIZE)
			{
				GWMMessage msg;
				msg.screenSizeResp.type = GWM_MSG_SCREEN_SIZE_RESP;
				msg.screenSizeResp.seq = cmd->screenSize.seq;
				msg.screenSizeResp.width = screen->width;
				msg.screenSizeResp.height = screen->height;
				_glidix_mqsend(guiQueue, info.pid, info.fd, &msg, sizeof(GWMMessage));
			}
			else if (cmd->cmd == GWM_CMD_SET_FLAGS)
			{
				pthread_mutex_lock(&windowLock);
				Window *win = GetWindowByID(cmd->setFlags.win, info.pid, info.fd);
				
				GWMMessage msg;
				msg.setFlagsResp.type = GWM_MSG_SET_FLAGS_RESP;
				msg.setFlagsResp.seq = cmd->setFlags.seq;
				
				if (win != NULL)
				{
					win->params.flags = cmd->setFlags.flags;
					if (win->params.flags & GWM_WINDOW_MKFOCUSED)
					{
						focusedWindow = win;
					};
					if ((win->params.flags & GWM_WINDOW_HIDDEN) == 0)
					{
						PostWindowDirty(win);
					};
					msg.setFlagsResp.status = 0;
				}
				else
				{
					msg.setFlagsResp.status = -1;
				};
				
				pthread_mutex_unlock(&windowLock);
				_glidix_mqsend(guiQueue, info.pid, info.fd, &msg, sizeof(GWMMessage));
				sem_post(&semRedraw);
			}
			else if (cmd->cmd == GWM_CMD_SET_CURSOR)
			{
				pthread_mutex_lock(&windowLock);
				Window *win = GetWindowByID(cmd->setCursor.win, info.pid, info.fd);
				
				GWMMessage msg;
				msg.setCursorResp.type = GWM_MSG_SET_CURSOR_RESP;
				msg.setCursorResp.seq = cmd->setCursor.seq;
				
				if ((win != NULL) && (cmd->setCursor.cursor >= 0) && (cmd->setCursor.cursor < GWM_CURSOR_COUNT))
				{
					win->cursor = cmd->setCursor.cursor;
					msg.setCursorResp.status = 0;
				}
				else
				{
					msg.setCursorResp.status = -1;
				};
				
				pthread_mutex_unlock(&windowLock);
				_glidix_mqsend(guiQueue, info.pid, info.fd, &msg, sizeof(GWMMessage));
				sem_post(&semRedraw);
			}
			else if (cmd->cmd == GWM_CMD_SET_ICON)
			{
				pthread_mutex_lock(&windowLock);
				Window *win = GetWindowByID(cmd->setIcon.win, info.pid, info.fd);
				
				GWMMessage msg;
				msg.setIconResp.type = GWM_MSG_SET_ICON_RESP;
				msg.setIconResp.seq = cmd->setIcon.seq;
				
				if (win != NULL)
				{
					if (win->icon != NULL) ddiDeleteSurface(win->icon);
					win->icon = ddiCreateSurface(&screenFormat, 16, 16, cmd->setIcon.data, 0);
					msg.setIconResp.status = 0;
				}
				else
				{
					msg.setIconResp.status = -1;
				};
				
				pthread_mutex_unlock(&windowLock);
				_glidix_mqsend(guiQueue, info.pid, info.fd, &msg, sizeof(GWMMessage));
				sem_post(&semRedraw);
			}
			else if (cmd->cmd == GWM_CMD_GET_FORMAT)
			{
				GWMMessage msg;
				msg.getFormatResp.type = GWM_MSG_GET_FORMAT_RESP;
				msg.getFormatResp.seq = cmd->getFormat.seq;
				memcpy(&msg.getFormatResp.format, &screenFormat, sizeof(DDIPixelFormat));
				_glidix_mqsend(guiQueue, info.pid, info.fd, &msg, sizeof(GWMMessage));
			}
			else if (cmd->cmd == GWM_CMD_GET_WINDOW_LIST)
			{
				pthread_mutex_lock(&windowLock);
				Window *win;
				
				GWMMessage msg;
				msg.getWindowListResp.type = GWM_MSG_GET_WINDOW_LIST_RESP;
				msg.getWindowListResp.seq = cmd->getWindowList.seq;
				msg.getWindowListResp.count = 0;
				
				for (win=desktopWindows; win!=NULL; win=win->next)
				{
					if (msg.getWindowListResp.count == 128) break;
					
					msg.getWindowListResp.wins[msg.getWindowListResp.count].id = win->id;
					msg.getWindowListResp.wins[msg.getWindowListResp.count].fd = win->fd;
					msg.getWindowListResp.wins[msg.getWindowListResp.count].pid = win->pid;
					msg.getWindowListResp.count++;
				};
				
				if (focusedWindow != NULL)
				{
					Window *foc = focusedWindow;
					while (foc->parent != NULL) foc = foc->parent;
					msg.getWindowListResp.focused.id = foc->id;
					msg.getWindowListResp.focused.fd = foc->fd;
					msg.getWindowListResp.focused.pid = foc->pid;
				}
				else
				{
					msg.getWindowListResp.focused.id = 0;
					msg.getWindowListResp.focused.fd = 0;
					msg.getWindowListResp.focused.pid = 0;
				};
				
				pthread_mutex_unlock(&windowLock);
				_glidix_mqsend(guiQueue, info.pid, info.fd, &msg, sizeof(GWMMessage));
			}
			else if (cmd->cmd == GWM_CMD_GET_WINDOW_PARAMS)
			{
				pthread_mutex_lock(&windowLock);
				Window *win = GetWindowByID(cmd->getWindowParams.ref.id, cmd->getWindowParams.ref.pid, cmd->getWindowParams.ref.fd);
				
				GWMMessage msg;
				msg.getWindowParamsResp.type = GWM_MSG_GET_WINDOW_PARAMS_RESP;
				msg.getWindowParamsResp.seq = cmd->getWindowParams.seq;
				if (win == NULL)
				{
					msg.getWindowParamsResp.status = -1;
				}
				else
				{
					msg.getWindowParamsResp.status = 0;
					memcpy(&msg.getWindowParamsResp.params, &win->params, sizeof(GWMWindowParams));
				};
				
				pthread_mutex_unlock(&windowLock);
				_glidix_mqsend(guiQueue, info.pid, info.fd, &msg, sizeof(GWMMessage));
				sem_post(&semRedraw);
			}
			else if (cmd->cmd == GWM_CMD_SET_LISTEN_WINDOW)
			{
				pthread_mutex_lock(&windowLock);
				listenWindow = GetWindowByID(cmd->setListenWindow.win, info.pid, info.fd);
				pthread_mutex_unlock(&windowLock);
			};
		}
		else if (info.type == _GLIDIX_MQ_HANGUP)
		{
			pthread_mutex_lock(&windowLock);
			DeleteWindowsOf(info.pid, info.fd);
			pthread_mutex_unlock(&windowLock);
			sem_post(&semRedraw);
		};
	};
};

int strStartsWith(const char *str, const char *prefix)
{
	if (strlen(str) < strlen(prefix))
	{
		return 0;
	};
	
	return memcmp(str, prefix, strlen(prefix)) == 0;
};

int main(int argc, char *argv[])
{
	const char *displayDevice = NULL;
	int modeSelected = -1;
	
	int i;
	for (i=1; i<argc; i++)
	{
		if (strStartsWith(argv[i], "--display="))
		{
			if (strlen(argv[i]) < 11)
			{
				fprintf(stderr, "The --display option expects a parameter\n");
				return 1;
			};
			
			displayDevice = &argv[i][10];
		}
		else if (strStartsWith(argv[i], "--mode="))
		{
			if (sscanf(&argv[i][7], "%d", &modeSelected) != 1)
			{
				fprintf(stderr, "The --mode option expects a number\n");
				return 1;
			};
			
			if (modeSelected < 0)
			{
				fprintf(stderr, "The --mode option expects a positive number\n");
				return 1;
			};
		}
		else if (strcmp(argv[i], "--usage") == 0)
		{
			fprintf(stderr, "USAGE:\t%s --display=<filename> --mode=<mode-number>\n", argv[0]);
			fprintf(stderr, "\tStarts the window manager on the specified display\n");
			fprintf(stderr, "\tdevice and in the given video mode.\n");
			return 1;
		}
		else
		{
			fprintf(stderr, "Unknown command-line parameter: %s\n", argv[i]);
			return 1;
		};
	};
	
	if (geteuid() != 0)
	{
		fprintf(stderr, "You need to be root to start the window manager\n");
		return 1;
	};
	
	if (displayDevice == NULL)
	{
		fprintf(stderr, "Please specify a display device using the --display option\n");
		return 1;
	};
	
	pthread_mutex_init(&windowLock, NULL);
	pthread_mutex_init(&mouseLock, NULL);
	sem_init(&semRedraw, 0, 0);
	
	int fd = open(displayDevice, O_RDWR);
	if (fd == -1)
	{
		fprintf(stderr, "Cannot open display device '%s': %s\n", displayDevice, strerror(errno));
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

	if (modeSelected == -1)
	{
		modeSelected = dstat.numModes - 1;
	};
	
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

	_glidix_kopt(_GLIDIX_KOPT_GFXTERM, 0);
	
	screenFormat.bpp = 4;
	screenFormat.redMask = 0xFF0000;
	screenFormat.greenMask = 0x00FF00;
	screenFormat.blueMask = 0x0000FF;
	screenFormat.alphaMask = 0xFF000000;
	screenFormat.pixelSpacing = 0;
	screenFormat.scanlineSpacing = 0;
	
	screenWidth = mode.width;
	screenHeight = mode.height;
	
	const char *fontError;
	captionFont = ddiLoadFont("DejaVu Sans", 12, DDI_STYLE_BOLD, &fontError);
	if (captionFont == NULL)
	{
		fprintf(stderr, "Failed to load caption font: %s\n", fontError);
		return 1;
	};
	
	frontBuffer = ddiCreateSurface(&screenFormat, mode.width, mode.height, videoram, DDI_STATIC_FRAMEBUFFER);
	screen = ddiCreateSurface(&screenFormat, mode.width, mode.height, NULL, 0);
	
	DDIColor white = {0xFF, 0xFF, 0xFF, 0xFF};
	ddiDrawText(frontBuffer, 5, 5, "GUI loading, please wait...", &white, NULL);

	DDIColor backgroundColor = {0, 0, 0xDD, 0xFF};
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

	for (i=0; i<GWM_CURSOR_COUNT; i++)
	{
		cursors[i].image = ddiLoadAndConvertPNG(&screenFormat, cursors[i].src, NULL);
		if (cursors[i].image == NULL)
		{
			cursors[i].image = ddiCreateSurface(&screenFormat, 16, 16, NULL, 0);
			ddiFillRect(cursors[i].image, 0, 0, 16, 16, &mouseColor);
		};
	};
	
	mouseX = mode.width / 2 - 8;
	mouseY = mode.height / 2 - 8;
	
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

	winCap = ddiLoadAndConvertPNG(&screenFormat, "/usr/share/images/wincap.png", NULL);
	if (winCap == NULL)
	{
		winCap = ddiCreateSurface(&screenFormat, 3, GUI_CAPTION_HEIGHT, NULL, 0);
		ddiFillRect(winButtons, 0, 0, 3, GUI_CAPTION_HEIGHT, &winDecoColor);
	};
	
	// for window borders
	winDeco = ddiCreateSurface(&screenFormat, screenWidth, screenHeight, NULL, 0);
	ddiFillRect(winDeco, 0, 0, screenWidth, screenHeight, &winDecoColor);
	winUnfoc = ddiCreateSurface(&screenFormat, screenWidth, screenHeight, NULL, 0);
	ddiFillRect(winUnfoc, 0, 0, screenWidth, screenHeight, &winUnfocColor);
	
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
	while (1)
	{
		//clock_t start = clock();
		PaintDesktop();
		//clock_t end = clock();
		
		//printf("render ms: %d\n", (int)((end-start)*1000/CLOCKS_PER_SEC));
		while (sem_wait(&semRedraw) != 0);
		
		// ignore multiple simultaneous redraw requests
		int val;
		sem_getvalue(&semRedraw, &val);
		while ((val--) > 0)
		{
			sem_wait(&semRedraw);
		};
	};
	
	return 0;
};
