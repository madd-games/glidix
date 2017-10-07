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

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <poll.h>

#include "window.h"
#include "screen.h"
#include "kblayout.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/**
 * Maximum frames per second.
 */
#define	FRAME_RATE				30

Window *desktopWindow;

pthread_mutex_t mouseLock = PTHREAD_MUTEX_INITIALIZER;
int cursorX, cursorY;

pthread_mutex_t invalidateLock = PTHREAD_MUTEX_INITIALIZER;

/**
 * Cached windows. Each one counts as a new reference so you must wndDown() when
 * setting to a different value, and you must wndUp() the new value.
 */
pthread_mutex_t wincacheLock;
Window *wndHovering = NULL;				// mouse hovers over this window
Window *wndFocused = NULL;				// main focused window
Window *wndActive = NULL;				// active window (currently clicked on)

/**
 * A semaphore which is signalled whenever the back buffer is updated, to let the swapping thread
 * do its job.
 */
sem_t semSwap;

typedef struct
{
	DDISurface* sprite;
	DDISurface* back;
	int hotX, hotY;
	const char *path;
} Cursor;

static Cursor cursors[GWM_CURSOR_COUNT] = {
	{NULL, NULL, 0, 0, "/usr/share/images/cursor.png"},
	{NULL, NULL, 7, 7, "/usr/share/images/txtcursor.png"},
	{NULL, NULL, 13, 3, "/usr/share/images/hand.png"}
};

void* swapThread(void *ignore)
{
	while (1)
	{
		sem_wait(&semSwap);
		
		// if we were signalled multiple times, merge it into one draw
		int value;
		sem_getvalue(&semSwap, &value);
		while (value--) sem_wait(&semSwap);
		
		// draw
		clock_t start = clock();
		pthread_mutex_lock(&invalidateLock);
		ddiOverlay(screen, 0, 0, frontBuffer, 0, 0, screen->width, screen->height);
		pthread_mutex_unlock(&invalidateLock);
		clock_t end = clock();
		
		// cap the framerate
		int msTaken = (int) ((end - start) * 1000 / CLOCKS_PER_SEC);
		int msNeeded = 1000 / FRAME_RATE;
		if (msTaken < msNeeded)
		{
			poll(NULL, 0, msNeeded - msTaken);
		};
	};
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
	desktopWindow->scrollX = 0;
	desktopWindow->scrollY = 0;
	desktopWindow->canvas = desktopBackground;
	desktopWindow->front = ddiCreateSurface(&screen->format, screen->width, screen->height,
						(char*)desktopBackground->data, 0);
	
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
		
		cursors[i].back = ddiCreateSurface(&screen->format, cursors[i].sprite->width, cursors[i].sprite->height, NULL, 0);
		if (cursors[i].back == NULL)
		{
			fprintf(stderr, "[gwmserver] failed to create back-cache for cursor %d\n", i);
			abort();
		};
	};
	
	cursorX = screen->width / 2;
	cursorY = screen->height / 2;
	
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&wincacheLock, &attr);
	
	sem_init(&semSwap, 0, 0);
	pthread_t thread;
	pthread_create(&thread, NULL, swapThread, NULL);
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
	
	if ((pars->flags & GWM_WINDOW_HIDDEN) == 0)
	{
		wndDirty(wnd);
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
	pthread_mutex_lock(&wincacheLock);
	Window *compare;
	for (compare=wndFocused; compare!=NULL; compare=compare->parent)
	{
		if (compare == wnd)
		{
			pthread_mutex_unlock(&wincacheLock);
			return 1;
		};
	};
	
	pthread_mutex_unlock(&wincacheLock);
	return 0;
};

#if 0
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
#endif

static void wndInvalidateWalk(Window *wnd, int cornerX, int cornerY, int invX, int invY, int width, int height)
{
	pthread_mutex_lock(&wnd->lock);
	int scrollX = wnd->scrollX;
	int scrollY = wnd->scrollY;
	
	Window *child = wnd->children;
	while (child != NULL)
	{
		wndUp(child);
		pthread_mutex_unlock(&wnd->lock);
		
		pthread_mutex_lock(&child->lock);
		
		if (child->params.flags & GWM_WINDOW_HIDDEN)
		{
			pthread_mutex_unlock(&child->lock);
			pthread_mutex_lock(&wnd->lock);
			Window *next = child->next;
			wndDown(child);
			child = next;
			continue;
		};
		
		// blit if the child overlaps the invalidated area
		int invEndX = invX + width;
		int invEndY = invY + height;
		
		int wndScreenX = cornerX + child->params.x - scrollX;
		int wndScreenY = cornerY + child->params.y - scrollY;
		
		int wndEndX = wndScreenX + child->params.width;
		int wndEndY = wndScreenY + child->params.height;
		
		int overlapX = MAX(wndScreenX, invX);
		int overlapY = MAX(wndScreenY, invY);
		
		int overlapEndX = MIN(wndEndX, invEndX);
		int overlapEndY = MIN(wndEndY, invEndY);
		
		if ((overlapX < overlapEndX) && (overlapY < overlapEndY))
		{
			// find the pixel on this window's front canvas
			int srcX = overlapX + scrollX - child->params.x - cornerX - child->scrollX;
			int srcY = overlapY + scrollY - child->params.y - cornerY - child->scrollY;
			
			// blit
			ddiBlit(child->front, srcX, srcY, screen, overlapX, overlapY, overlapEndX - overlapX, overlapEndY - overlapY);
			
			// do it to all children
			pthread_mutex_unlock(&child->lock);
			wndInvalidateWalk(child, wndScreenX, wndScreenY, overlapX, overlapY, overlapEndX - overlapX, overlapEndY - overlapY);
		}
		else
		{
			// nothing to do
			pthread_mutex_unlock(&child->lock);
		};
		
		// re-lock the parent to allow us to traverse the list
		pthread_mutex_lock(&wnd->lock);
		Window *next = child->next;
		wndDown(child);
		child = next;
	};
	
	pthread_mutex_unlock(&wnd->lock);
};

void wndInvalidate(int x, int y, int width, int height)
{
	pthread_mutex_lock(&invalidateLock);
	
	// first overlay the correct background fragment onto the screen
	ddiOverlay(desktopBackground, x, y, screen, x, y, width, height);
	
	// now draw whatever windows are on top of it
	wndInvalidateWalk(desktopWindow, 0, 0, x, y, width, height);
	
	// re-draw the overlapping part of the mouse cursor
	pthread_mutex_lock(&mouseLock);
	
	pthread_mutex_lock(&wincacheLock);
	int cursor = 0;
	if (wndHovering != NULL) cursor = wndHovering->cursor;
	pthread_mutex_unlock(&wincacheLock);
	
	int cx = cursorX - cursors[cursor].hotX;
	int cy = cursorY - cursors[cursor].hotY;
	
	int left = MAX(x, cx);
	int right = MIN(x+width, cx+cursors[cursor].sprite->width);
	int top = MAX(y, cy);
	int bottom = MIN(y+height, cy+cursors[cursor].sprite->height);
	
	if (left < right && top < bottom)
	{
		ddiOverlay(screen, left, top, cursors[cursor].back, left - cx, top - cy, right - left, bottom - top);
		ddiBlit(cursors[cursor].sprite, left - cx, top - cy, screen, left, top, right - left, bottom - top);
	};
	
	pthread_mutex_unlock(&mouseLock);
	
	pthread_mutex_unlock(&invalidateLock);
};

void wndDirty(Window *wnd)
{
	// figure out our absolute coordinates then call wndInvalidate() on that region
	int x, y;
	if (wndRelToAbs(wnd, wnd->scrollX, wnd->scrollY, &x, &y) != 0) return;
	
	int width, height;
	pthread_mutex_lock(&wnd->lock);
	width = wnd->params.width;
	height = wnd->params.height;
	pthread_mutex_unlock(&wnd->lock);
	
	wndInvalidate(x, y, width, height);
};

void wndDrawScreen()
{
	sem_post(&semSwap);
};

void wndMouseMove(int x, int y)
{
	pthread_mutex_lock(&invalidateLock);
	pthread_mutex_lock(&mouseLock);

	pthread_mutex_lock(&wincacheLock);
	int oldCursor = 0;
	if (wndHovering != NULL) oldCursor = wndHovering->cursor;
	pthread_mutex_unlock(&wincacheLock);
	
	ddiOverlay(cursors[oldCursor].back, 0, 0, screen, cursorX - cursors[oldCursor].hotX, cursorY - cursors[oldCursor].hotY, cursors[oldCursor].back->width, cursors[oldCursor].back->height);
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

	int newCursor = 0;
	if (wndHovering != NULL) newCursor = wndHovering->cursor;
	ddiOverlay(screen, cursorX - cursors[newCursor].hotX, cursorY - cursors[newCursor].hotY, cursors[newCursor].back, 0, 0, cursors[newCursor].back->width, cursors[newCursor].back->height);
	ddiBlit(cursors[newCursor].sprite, 0, 0, screen, cursorX - cursors[newCursor].hotX, cursorY - cursors[newCursor].hotY, cursors[newCursor].sprite->width, cursors[newCursor].sprite->height);

	pthread_mutex_unlock(&wincacheLock);
	pthread_mutex_unlock(&mouseLock);
	pthread_mutex_unlock(&invalidateLock);
	wndDrawScreen();
};

void wndAbsToRelWithin(Window *win, int x, int y, int *relX, int *relY)
{
	pthread_mutex_lock(&win->lock);
	int expX, expY;
	if (win->parent != NULL)
	{
		expX = x - win->params.x + win->parent->scrollX;
		expY = y - win->params.y + win->parent->scrollY;
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
	if ((winX >= win->params.width) || (winY >= win->params.height) || (winX < 0) || (winY < 0))
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
			candidate = wndAbsToRelFrom(child, winX - child->params.x + win->scrollX,
							winY - child->params.y + win->scrollY, relX, relY);
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

int wndRelToAbs(Window *win, int relX, int relY, int *absX, int *absY)
{
	if (win == desktopWindow)
	{
		*absX = relX;
		*absY = relY;
		return 0;
	};
	
	pthread_mutex_lock(&win->lock);
	int cornerX = win->params.x;
	int cornerY = win->params.y;
	int scrollX = win->scrollX;
	int scrollY = win->scrollY;
	Window *parent = win->parent;
	if (parent != NULL) wndUp(parent);
	pthread_mutex_unlock(&win->lock);
	
	if (parent != NULL)
	{
		int result = wndRelToAbs(win->parent, cornerX + relX - scrollX, cornerY + relY - scrollY, absX, absY);
		wndDown(parent);
		return result;
	}
	else return -1;
};
