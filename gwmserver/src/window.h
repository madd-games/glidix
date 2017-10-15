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

#ifndef WINDOW_H
#define WINDOW_H

#include <inttypes.h>
#include <libgwm.h>
#include <pthread.h>

/**
 * Properties of window decorations.
 */
#define WINDOW_CAPTION_HEIGHT			20
#define	WINDOW_BORDER_WIDTH			5

/**
 * Describes a window.
 */
typedef struct Window_
{
	/**
	 * Links. The reference count of a child is to be incremented before the parent's
	 * lock is released. The prev/next fields are protected by the parent's lock.
	 * The children list is protected by this window's own lock. The parent field is
	 * protected by this window's own, and is NULL if the window has no parent.
	 * The parent is only changed when the window is "destroyed" (removed from child
	 * list) or reparented.
	 */
	struct Window_*					prev;
	struct Window_*					next;
	struct Window_*					parent;
	struct Window_*					children;

	/**
	 * Lock for this window.
	 */
	pthread_mutex_t					lock;
	
	/**
	 * Reference count of the window.
	 */
	int						refcount;
	
	/**
	 * Window parameters.
	 */
	GWMWindowParams					params;
	
	/**
	 * Front and back canvases of the window.
	 */
	DDISurface*					canvas;
	DDISurface*					front;

	/**
	 * The icon (or NULL).
	 */
	DDISurface*					icon;
	
	/**
	 * Window ID assigned by the application, and the file descriptor referring to the client
	 * connection, used to dispatch events. If 'fd' is zero, then the window is not attached
	 * to any application ("destroyed") and will ignore events.
	 */
	uint64_t					id;
	int						fd;
	
	/**
	 * Scroll position.
	 */
	int						scrollX;
	int						scrollY;
	
	/**
	 * The cursor to display for this window. Atomic read/write - no lock necessary.
	 */
	int						cursor;
	
	/**
	 * 1 if this window is decorated (its parent is a "decoration window"), 0 otherwise.
	 */
	int						decorated;
	
	/**
	 * 0 if this window is a decoration.
	 */
	int						isDecoration;
} Window;

extern Window* desktopWindow;

/**
 * Initialize the windowing subsystem.
 */
void wndInit();

/**
 * Create a new window with the specified parent. It gets added to the child list, and also returned,
 * so the reference count becomes 2.
 */
Window* wndCreate(Window *parent, GWMWindowParams *param, uint64_t id, int fd, DDISurface *canvas);

/**
 * Increase the reference count of a window.
 */
void wndUp(Window *wnd);

/**
 * Decrease the reference count of a window. If it reaches zero, the window is freed.
 */
void wndDown(Window *wnd);

/**
 * Remove a window. This disconnects it from its parent, and decreases the refcount of the window itself,
 * and also its parent (as the number of children is added to the refcount to prevent them from being
 * automatically deleted). Returns 0 on success or a GWM error code (GWM_ERR_*) on failure.
 */
int wndDestroy(Window *wnd);

/**
 * Re-render a specific region of the screen.
 */
void wndInvalidate(int x, int y, int width, int height);

/**
 * Make the specified window dirty. This causes it to be rendered to the screen.
 */
void wndDirty(Window *wnd);

/**
 * Draw the screen (call after wndDirty()).
 */
void wndDrawScreen();

/**
 * Update the mouse cursor - set coordinates, dispatch event, redraw.
 */
void wndMouseMove(int x, int y);

/**
 * Convert absolute coordinates to coordinates relative to a window.
 */
void wndAbsToRelWithin(Window *wnd, int x, int y, int *relX, int *relY);

/**
 * Convert absolute coordinates to relative coordinates. Return the window relative to which the coordinates
 * were determined. The window is upreffed. This enver returns NULL (but may return the desktop window).
 */
Window* wndAbsToRel(int absX, int absY, int *relX, int *relY);

/**
 * Convert relative coordinates to absolute. Returns 0 on success, or -1 if the window is not visible.
 */
int wndRelToAbs(Window *win, int relX, int relY, int *absX, int *absY);

/**
 * Send an event to a window. The event is ignored if the window is not attached to an application.
 */
void wndSendEvent(Window *win, GWMEvent *ev);

/**
 * Call this when the left mouse button has been pressed.
 */
void wndOnLeftDown();

/**
 * Call this when the left mouse button has been released.
 */
void wndOnLeftUp();

/**
 * Dispatch an input event to the focused window.
 */
void wndInputEvent(int type, int scancode, int keycode);

/**
 * Set window flags.
 */
void wndSetFlags(Window *wnd, int flags);

/**
 * Set the icon of a window to the specified surface ID. Return 0 on success, or a GWM error number
 * on error.
 */
int wndSetIcon(Window *wnd, uint32_t surfID);

/**
 * Draw the window decoration for 'child', where 'decor' is its decoration window.
 */
void wndDecorate(Window *decor, Window *child);

/**
 * Get the list of windows. Used to implement the GWM_CMD_GET_WINDOW_LIST command. The global reference
 * of the focused window's top-level application-visible parent is stored in 'focused', and the references
 * of all other windows on the desktop (their decorated child if they are decorations) is stored in
 * 'list'. The list must have 128 entries. 'outCount' points to where the actual number of windows will
 * be stored.
 */
void wndGetWindowList(GWMGlobWinRef *focused, GWMGlobWinRef *list, int *outCount);

/**
 * Given a window reference, return the parameters of a window, if it's on the desktop. Returns 0 on success,
 * or an error number (usually GWM_ERR_NOWND) on error.
 */
int wndGetWindowParams(GWMGlobWinRef *ref, GWMWindowParams *params);

/**
 * Set the specified window as the "desktop listening window", which receives desktop update events if the list
 * of windows on the desktop changes. Mainly used to implement the system bar (sysbar).
 */
void wndSetListenWindow(Window *win);

/**
 * Send the desktop update event.
 */
void wndDesktopUpdate();

/**
 * "Toggle" a window. This handles what should happen when someone clicks on it on the system bar:
 *  - If it not hidden, minimize it (set GWM_WINDOW_HIDDEN) and make not focused.
 *  - If hidden, make focused and not hidden.
 */
int wndToggle(GWMGlobWinRef *ref);

#endif
