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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "window.h"
#include "screen.h"
#include "kblayout.h"

#define	WINDOW_BORDER_WIDTH				3
#define	WINDOW_CAPTION_HEIGHT				20

Window *desktopWindow;

pthread_mutex_t mouseLock = PTHREAD_MUTEX_INITIALIZER;
int cursorX, cursorY;

/**
 * Cached windows. Each one counts as a new reference so you must wndDown() when
 * setting to a different value, and you must wndUp() the new value.
 */
pthread_mutex_t wincacheLock;
Window *wndHovering = NULL;				// mouse hovers over this window
Window *wndFocused = NULL;				// main focused window
Window *wndActive = NULL;				// active window (currently clicked on)

typedef struct
{
	DDISurface* sprite;
	int hotX, hotY;
	const char *path;
} Cursor;

static Cursor cursors[GWM_CURSOR_COUNT] = {
	{NULL, 0, 0, "/usr/share/images/cursor.png"},
	{NULL, 7, 7, "/usr/share/images/txtcursor.png"},
	{NULL, 13, 3, "/usr/share/images/hand.png"}
};

void wndInit()
{
	desktopWindow = (Window*) malloc(sizeof(Window));
	memset(desktopWindow, 0, sizeof(Window));
	pthread_mutex_init(&desktopWindow->lock, NULL);
	desktopWindow->refcount = 1;
	desktopWindow->params.flags = GWM_WINDOW_NODECORATE;
	desktopWindow->params.width = screen->width;
	desktopWindow->params.height = screen->height;
	desktopWindow->fgScrollX = -WINDOW_BORDER_WIDTH;
	desktopWindow->fgScrollY = -WINDOW_CAPTION_HEIGHT;
	desktopWindow->canvas = desktopBackground;
	desktopWindow->front = ddiCreateSurface(&screen->format, screen->width, screen->height,
						(char*)desktopBackground->data, 0);
	desktopWindow->display = screen;
	
	int i;
	for (i=0; i<GWM_CURSOR_COUNT; i++)
	{
		const char *error;
		cursors[i].sprite = ddiLoadAndConvertPNG(&screen->format, cursors[i].path, &error);
		if (cursors[i].sprite == NULL)
		{
			fprintf(stderr, "[gwmserver] cannot load cursor %d from %s: %s\n", i, cursors[i].path, error);
			abort();
		};
	};
	
	cursorX = screen->width / 2;
	cursorY = screen->height / 2;
	
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&wincacheLock, &attr);
};

Window* wndCreate(Window *parent, GWMWindowParams *pars, uint64_t id, int fd, DDISurface *canvas)
{
	Window *wnd = (Window*) malloc(sizeof(Window));
	memset(wnd, 0, sizeof(Window));
	pthread_mutex_init(&wnd->lock, NULL);
	wnd->refcount = 2;		// returned handle + on the child list
	memcpy(&wnd->params, pars, sizeof(GWMWindowParams));
	
	wnd->canvas = canvas;
	wnd->front = ddiCreateSurface(&canvas->format, canvas->width, canvas->height, (char*)canvas->data, 0);
	wnd->display = ddiCreateSurface(&screen->format, pars->width, pars->height, NULL, DDI_SHARED);
	
	wnd->icon = NULL;
	
	wnd->id = id;
	wnd->fd = fd;
	
	// add to list
	wndUp(parent);
	pthread_mutex_lock(&parent->lock);
	if (parent->children == NULL)
	{
		parent->children = wnd;
	}
	else
	{
		Window *last = parent->children;
		while (last->next != NULL) last = last->next;
		last->next = wnd;
		wnd->prev = last;
	};
	wnd->parent = parent;
	pthread_mutex_unlock(&parent->lock);
	
	if (pars->flags & GWM_WINDOW_MKFOCUSED)
	{
		pthread_mutex_lock(&wincacheLock);
		if (wndFocused != NULL)
		{
			GWMEvent ev;
			memset(&ev, 0, sizeof(GWMEvent));
			ev.type = GWM_EVENT_FOCUS_OUT;
			ev.win = wndFocused->id;
			wndSendEvent(wndFocused, &ev);
			wndDown(wndFocused);
		};
		wndUp(wnd);
		wndFocused = wnd;
		pthread_mutex_unlock(&wincacheLock);
	};
	
	return wnd;
};

void wndUp(Window *wnd)
{
	__sync_fetch_and_add(&wnd->refcount, 1);
};

void wndDown(Window *wnd)
{
	if (__sync_add_and_fetch(&wnd->refcount, -1) == 0)
	{
		ddiDeleteSurface(wnd->canvas);
		ddiDeleteSurface(wnd->front);
		ddiDeleteSurface(wnd->display);
		free(wnd);
	};
};

int wndDestroy(Window *wnd)
{
	// get the parent and set it to NULL.
	pthread_mutex_lock(&wnd->lock);
	Window *parent = wnd->parent;
	wnd->parent = NULL;
	wnd->fd = 0;
	pthread_mutex_unlock(&wnd->lock);
	
	if (parent == NULL) return GWM_ERR_NOWND;
	
	// remove us from the list
	pthread_mutex_lock(&parent->lock);
	if (wnd->prev == NULL)
	{
		parent->children = wnd->next;
	}
	else
	{
		wnd->prev->next = wnd->next;
	};
	
	if (wnd->next != NULL)
	{
		wnd->next->prev = wnd->prev;
	};
	pthread_mutex_unlock(&parent->lock);
	wndDirty(parent);
	wndDown(parent);
	
	pthread_mutex_lock(&wincacheLock);
	if (wndHovering == wnd)
	{
		wndDown(wnd);
		wndHovering = NULL;
	};
	if (wndFocused == wnd)
	{
		wndDown(wnd);
		wndFocused = NULL;
	};
	if (wndActive == wnd)
	{
		wndDown(wnd);
		wndActive = NULL;
	};
	pthread_mutex_unlock(&wincacheLock);
	
	wndDown(wnd);
	
	return 0;
};

int wndIsFocused(Window *wnd)
{
	//pthread_mutex_lock(&wincacheLock);
	Window *compare;
	for (compare=wndFocused; compare!=NULL; compare=compare->parent)
	{
		if (compare == wnd)
		{
			//pthread_mutex_unlock(&wincacheLock);
			return 1;
		};
	};
	
	//pthread_mutex_unlock(&wincacheLock);
	return 0;
};

void wndDrawDecoration(DDISurface *target, Window *wnd)
{
	int active = wndIsFocused(wnd);
	
	DDIColor *bgColor;
	if (active)
	{
		bgColor = &gwminfo->colWinActive;
	}
	else
	{
		bgColor = &gwminfo->colWinInactive;
	};
	
	ddiFillRect(target, wnd->params.x, wnd->params.y+WINDOW_CAPTION_HEIGHT,
			wnd->params.width+2*WINDOW_BORDER_WIDTH, wnd->params.height+WINDOW_BORDER_WIDTH,
			bgColor);
	
	int dy;
	if (active)
	{
		dy = 0;
	}
	else
	{
		dy = WINDOW_CAPTION_HEIGHT;
	};
	
	int sideWidth = (imgWincap->width - 1) / 2;
	int winWidth = wnd->params.width + 2 * WINDOW_BORDER_WIDTH;
	ddiBlit(imgWincap, 0, dy, target, wnd->params.x, wnd->params.y, sideWidth, WINDOW_CAPTION_HEIGHT);
	
	int dx;
	for (dx=sideWidth; dx<winWidth-sideWidth; dx++)
	{
		ddiBlit(imgWincap, sideWidth, dy, target, wnd->params.x+dx, wnd->params.y, 1, WINDOW_CAPTION_HEIGHT);
	};
	
	ddiBlit(imgWincap, imgWincap->width-sideWidth, dy, target, wnd->params.x+winWidth-sideWidth,
		wnd->params.y, sideWidth, WINDOW_CAPTION_HEIGHT);
	
	if (wnd->icon != NULL)
	{
		ddiBlit(wnd->icon, 0, 0, target, wnd->params.x+2, wnd->params.y+2, 16, 16);
	};
};

void wndDirty(Window *wnd)
{
	pthread_mutex_lock(&wnd->lock);
	Window *parent = wnd->parent;
	if (parent != NULL) wndUp(parent);
	
	ddiOverlay(wnd->front, wnd->bgScrollX, wnd->bgScrollY, wnd->display, 0, 0, wnd->params.width, wnd->params.height);
	
	Window *child;
	for (child=wnd->children; child!=NULL; child=child->next)
	{
		pthread_mutex_lock(&child->lock);
		if ((child->params.flags & GWM_WINDOW_HIDDEN) == 0)
		{
			if ((wnd == desktopWindow) && ((child->params.flags & GWM_WINDOW_NODECORATE) == 0))
			{
				wndDrawDecoration(wnd->display, child);
			};
			
			ddiBlit(child->display, 0, 0, wnd->display,
				child->params.x-wnd->fgScrollX, child->params.y-wnd->fgScrollY,
				child->params.width, child->params.height);
		};
		pthread_mutex_unlock(&child->lock);
	};
	
	pthread_mutex_unlock(&wnd->lock);
	
	if (parent != NULL)
	{
		wndDirty(parent);
		wndDown(parent);
	};
};

void wndDrawScreen()
{
	int cursor = 0;
	pthread_mutex_lock(&wincacheLock);
	if (wndHovering != NULL) cursor = wndHovering->cursor;
	pthread_mutex_unlock(&wincacheLock);
	
	int cx, cy;
	pthread_mutex_lock(&mouseLock);
	cx = cursorX;
	cy = cursorY;
	pthread_mutex_unlock(&mouseLock);
	
	pthread_mutex_lock(&desktopWindow->lock);
	ddiOverlay(screen, 0, 0, frontBuffer, 0, 0, screen->width, screen->height);
	ddiBlit(cursors[cursor].sprite, 0, 0, frontBuffer, cx - cursors[cursor].hotX, cy - cursors[cursor].hotY,
		cursors[cursor].sprite->width, cursors[cursor].sprite->height);
	pthread_mutex_unlock(&desktopWindow->lock);
};

void wndMouseMove(int x, int y)
{
	pthread_mutex_lock(&mouseLock);
	cursorX = x;
	cursorY = y;
	
	int relX, relY;
	Window *wnd = wndAbsToRel(x, y, &relX, &relY);
	
	pthread_mutex_lock(&wincacheLock);
	if (wndActive != NULL)
	{
		wndAbsToRelWithin(wndActive, x, y, &relX, &relY);
		GWMEvent ev;
		memset(&ev, 0, sizeof(GWMEvent));
		ev.type = GWM_EVENT_MOTION;
		ev.win = wndActive->id;
		ev.x = relX;
		ev.y = relY;
		wndSendEvent(wndActive, &ev);
		
		// we don't need the "hovered window"
		if (wnd != NULL) wndDown(wnd);
	}
	else
	{
		if (wnd != wndHovering)
		{
			if (wndHovering != NULL)
			{
				// leave event
				GWMEvent ev;
				memset(&ev, 0, sizeof(GWMEvent));
				ev.type = GWM_EVENT_LEAVE;
				ev.win = wndHovering->id;
				wndSendEvent(wndHovering, &ev);
				wndDown(wndHovering);
			};
		
			// enter event for the newly-hovered window
			GWMEvent ev;
			memset(&ev, 0, sizeof(GWMEvent));
			ev.type = GWM_EVENT_ENTER;
			ev.win = wnd->id;
			ev.x = relX;
			ev.y = relY;
			wndSendEvent(wnd, &ev);
			wndHovering = wnd;	// upreffed by wndAbsToRel()
		}
		else
		{
			if (wnd != NULL)
			{
				GWMEvent ev;
				memset(&ev, 0, sizeof(GWMEvent));
				ev.type = GWM_EVENT_MOTION;
				ev.win = wnd->id;
				ev.x = relX;
				ev.y = relY;
				wndSendEvent(wnd, &ev);
				wndDown(wnd);
			};
		};
	};
	
	pthread_mutex_unlock(&wincacheLock);
	pthread_mutex_unlock(&mouseLock);
	wndDrawScreen();
};

void wndAbsToRelWithin(Window *win, int x, int y, int *relX, int *relY)
{
	pthread_mutex_lock(&win->lock);
	int expX, expY;
	if (win->parent != NULL)
	{
		expX = x - win->params.x + win->parent->fgScrollX;
		expY = y - win->params.y + win->parent->fgScrollY;
	}
	else
	{
		expX = x - win->params.x;
		expY = y - win->params.y;
	};
	
	Window *parent = win->parent;
	if (parent != NULL) wndUp(parent);
	pthread_mutex_unlock(&win->lock);
	
	if (parent != NULL)
	{
		wndAbsToRelWithin(parent, expX, expY, relX, relY);
		wndDown(parent);
	}
	else
	{
		*relX = expX;
		*relY = expY;
	};
};

static Window* wndAbsToRelFrom(Window *win, int winX, int winY, int *relX, int *relY)
{
	// don't bother if outside bounds
	if ((winX >= win->params.width) || (winY >= win->params.height))
	{
		return NULL;
	};
	
	// try all the children
	Window *child;
	Window *lastCandidate = NULL;
	for (child=win->children; child!=NULL; child=child->next)
	{
		pthread_mutex_lock(&child->lock);
		Window *candidate;
		if (child->params.flags & GWM_WINDOW_HIDDEN)
		{
			candidate = NULL;
		}
		else
		{
			candidate = wndAbsToRelFrom(child, winX - child->params.x + win->fgScrollX,
							winY - child->params.y + win->fgScrollY, relX, relY);
		};
		pthread_mutex_unlock(&child->lock);
		if (candidate != NULL)
		{
			if (lastCandidate != NULL) wndDown(lastCandidate);
			lastCandidate = candidate;
		};
	};
	
	if (lastCandidate == NULL)
	{
		// it's us
		if (relX != NULL) *relX = winX;
		if (relY != NULL) *relY = winY;
		wndUp(win);
		return win;
	}
	else
	{
		return lastCandidate;
	};
};

Window* wndAbsToRel(int absX, int absY, int *relX, int *relY)
{
	pthread_mutex_lock(&desktopWindow->lock);
	Window *result = wndAbsToRelFrom(desktopWindow, absX, absY, relX, relY);
	pthread_mutex_unlock(&desktopWindow->lock);
	assert(result != NULL);
	return result;
};

void wndSendEvent(Window *win, GWMEvent *ev)
{
	GWMMessage msg;
	msg.event.type = GWM_MSG_EVENT;
	msg.event.seq = 0;
	memcpy(&msg.event.payload, ev, sizeof(GWMEvent));
	
	pthread_mutex_lock(&win->lock);
	if (win->fd != 0) write(win->fd, &msg, sizeof(GWMMessage));
	pthread_mutex_unlock(&win->lock);
};

void wndSetFocused(Window *wnd)
{
	if (wnd != NULL)
	{
		if (wndFocused != wnd)
		{
			wndUp(wnd);
			if (wndFocused != NULL)
			{
				GWMEvent ev;
				memset(&ev, 0, sizeof(GWMEvent));
				ev.type = GWM_EVENT_FOCUS_OUT;
				ev.win = wndFocused->id;
				wndSendEvent(wndFocused, &ev);
				wndDown(wndFocused);
			};
		
			wndFocused = wnd;

			GWMEvent ev;
			memset(&ev, 0, sizeof(GWMEvent));
			ev.type = GWM_EVENT_FOCUS_IN;
			ev.win = wndFocused->id;
			wndSendEvent(wndFocused, &ev);
			
			// move the top-level window to front
			if (wnd->parent != NULL)
			{
				while (wnd->parent->parent != NULL)
				{
					wnd = wnd->parent;
				};
				
				pthread_mutex_lock(&wnd->parent->lock);
				if (wnd->next != NULL)
				{
					wnd->next->prev = wnd->prev;
				
					if (wnd->prev == NULL)
					{
						wnd->parent->children = wnd->next;
					}
					else
					{
						wnd->prev->next = wnd->next;
					};
				
					Window *last = wnd->next;
					while (last->next != NULL) last = last->next;
				
					last->next = wnd;
					wnd->prev = last;
					wnd->next = NULL;
				};
				pthread_mutex_unlock(&wnd->parent->lock);
				
				wndDirty(desktopWindow);
				wndDrawScreen();
			};
		};
	}
	else
	{
		if (wndFocused != NULL)
		{
			GWMEvent ev;
			memset(&ev, 0, sizeof(GWMEvent));
			ev.type = GWM_EVENT_FOCUS_OUT;
			ev.win = wndFocused->id;
			wndSendEvent(wndFocused, &ev);
			wndDown(wndFocused);
		};
		wndFocused = NULL;
		
		wndDirty(desktopWindow);
		wndDrawScreen();
	};
};

void wndOnLeftDown()
{
	pthread_mutex_lock(&wincacheLock);
	if (wndHovering != NULL)
	{
		wndUp(wndHovering);
		if (wndActive != NULL) wndDown(wndActive);
		wndActive = wndHovering;
		
		wndSetFocused(wndHovering);
	}
	else
	{
		wndSetFocused(NULL);
	};
	pthread_mutex_unlock(&wincacheLock);
};

void wndOnLeftUp()
{
	pthread_mutex_lock(&wincacheLock);
	if (wndActive != NULL)
	{
		wndDown(wndActive);
		wndActive = NULL;
	};
	pthread_mutex_unlock(&wincacheLock);
};

void wndInputEvent(int type, int scancode, int keycode)
{
	int cx, cy;
	pthread_mutex_lock(&mouseLock);
	cx = cursorX;
	cy = cursorY;
	pthread_mutex_unlock(&mouseLock);
	
	pthread_mutex_lock(&wincacheLock);
	if (wndFocused != NULL)
	{
		GWMEvent ev;
		memset(&ev, 0, sizeof(GWMEvent));
		ev.type = type;
		ev.win = wndFocused->id;
		ev.scancode = scancode;
		ev.keycode = keycode;
		wndAbsToRelWithin(wndFocused, cx, cy, &ev.x, &ev.y);
		kblTranslate(&ev.keycode, &ev.keychar, &ev.keymod, type);
		wndSendEvent(wndFocused, &ev);
	};
	pthread_mutex_unlock(&wincacheLock);
};

void wndSetFlags(Window *wnd, int flags)
{
	pthread_mutex_lock(&wnd->lock);
	wnd->params.flags = flags;
	pthread_mutex_unlock(&wnd->lock);
	
	if (flags & GWM_WINDOW_MKFOCUSED)
	{
		pthread_mutex_lock(&wincacheLock);
		wndSetFocused(wnd);
		pthread_mutex_unlock(&wincacheLock);
	};
	
	wndDirty(wnd);
};

int wndSetIcon(Window *wnd, uint32_t surfID)
{
	int status = 0;
	
	pthread_mutex_lock(&wnd->lock);
	if (wnd->icon != NULL) ddiDeleteSurface(wnd->icon);
	wnd->icon = ddiOpenSurface(surfID);
	if (wnd->icon == NULL) status = GWM_ERR_NOSURF;
	pthread_mutex_unlock(&wnd->lock);
	
	wndDirty(desktopWindow);
	wndDrawScreen();
	return status;
};
