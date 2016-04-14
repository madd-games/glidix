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

#ifndef GUI_H_
#define GUI_H_

#include <stdint.h>
#include <libddi.h>

#define	GWM_WINDOW_HIDDEN			(1 << 0)
#define	GWM_WINDOW_NODECORATE			(1 << 1)
#define	GWM_WINDOW_MKFOCUSED			(1 << 2)

/**
 * Those must match up with the Glidix humin interface (<glidix/humin.h>).
 */
#define	GWM_SC_MOUSE_LEFT			0x100
#define	GWM_SC_MOUSE_MIDDLE			0x101
#define	GWM_SC_MOUSE_RIGHT			0x102

/**
 * Window parameter structure, as passed by clients to the window manager.
 */
typedef struct
{
	char					caption[1024];
	char					iconName[1024];
	unsigned int				flags;
	unsigned int				width;
	unsigned int				height;
	unsigned int				x;
	unsigned int				y;
} GWMWindowParams;

/**
 * Flags for @keymod.
 */
#define	GWM_KM_CTRL				(1 << 0)
#define	GWM_KM_SHIFT				(1 << 1)

/**
 * Event structure.
 */
#define	GWM_EVENT_CLOSE				0
#define	GWM_EVENT_DOWN				1
#define	GWM_EVENT_UP				2
#define	GWM_EVENT_UPDATE			3
typedef struct
{
	int					type;
	uint64_t				win;		// window ID
	int					x;		// mouse position relative to window
	int					y;
	int					scancode;	// hardware-specific key scancode
	int					keycode;	// hardware-independent key code
	int					keymod;		// bitwise-OR of active modifiers (GWM_KM_*)
	uint64_t				keychar;	// actual Unicode character entered by key, or zero
} GWMEvent;

/**
 * Commands understood by the window manager.
 */
#define	GWM_CMD_CREATE_WINDOW			0
#define	GWM_CMD_POST_DIRTY			1
#define	GWM_CMD_DESTROY_WINDOW			2
#define	GWM_CMD_CLEAR_WINDOW			3
typedef union
{
	int					cmd;
	
	struct
	{
		int				cmd;	// GWM_CMD_CREATE_WINDOW
		uint64_t			id;
		uint64_t			parent;
		GWMWindowParams			pars;
		uint64_t			seq;	// sequence number for responses
		int				painterPid;
	} createWindow;
	
	struct
	{
		int				cmd;	// GWM_CMD_DESTROY_WINDOW
		uint64_t			id;
	} destroyWindow;
	
	struct
	{
		int				cmd;	// GWM_CMD_CLEAR_WINDOW
		uint64_t			id;
		uint64_t			seq;
	} clearWindow;
} GWMCommand;

/**
 * Messages sent by the window manager to clients.
 */
#define GWM_MSG_CREATE_WINDOW_RESP		0
#define	GWM_MSG_EVENT				1
#define	GWM_MSG_CLEAR_WINDOW_RESP		2
typedef union
{
	struct
	{
		int				type;
		uint64_t			seq;	// sequence number of command being responded to.
	} generic;
	
	struct
	{
		int				type;	// GWM_MSG_CREATE_WINDOW_RESP
		uint64_t			seq;
		int				status;	// 0 = success, everything else = error
		DDIPixelFormat			format;
		uint64_t			shmemID;
		uint64_t			shmemSize;
		unsigned int			width;
		unsigned int			height;
	} createWindowResp;
	
	struct
	{
		int				type;	// GWM_MSG_EVENT
		uint64_t			seq;	// always zero for events
		GWMEvent			payload;
	} event;
	
	struct
	{
		int				type;	// GWM_MSG_CLEAR_WINDOW_RESP
		uint64_t			seq;
	} clearWindowResp;
} GWMMessage;

/**
 * Describes a window on the application side.
 */
typedef struct
{
	uint64_t				id;
	uint64_t				shmemAddr;
	uint64_t				shmemSize;
	DDISurface*				canvas;
} GWMWindow;

/**
 * Initialises the GWM library. This must be called before using any other functions.
 * Returns 0 on success, or -1 on error.
 */
int gwmInit();

/**
 * Quit the GWM library, with proper cleanup.
 */
void gwmQuit();

/**
 * Creates a new window. On success, returns a window handle; on error, returns NULL.
 */
GWMWindow* gwmCreateWindow(
	GWMWindow *parent,
	const char *caption,
	unsigned int x, unsigned int y,
	unsigned int width, unsigned int height,
	int flags);

/**
 * Return the DDI surface representing the window.
 */
DDISurface* gwmGetWindowCanvas(GWMWindow *win);

/**
 * Destroy a window.
 */
void gwmDestroyWindow(GWMWindow *win);

/**
 * Tell the window manager that the screen needs re-drawing.
 */
void gwmPostDirty();

/**
 * Wait until an event is received and store it in the event structure.
 */
void gwmWaitEvent(GWMEvent *ev);

/**
 * Clear a window.
 */
void gwmClearWindow(GWMWindow *win);

/**
 * Post an update event. The window is allowed to be NULL.
 */
void gwmPostUpdate(GWMWindow *win);

#endif
