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
#define	GWM_WINDOW_NOTASKBAR			(1 << 3)
#define	GWM_WINDOW_NOSYSMENU			(1 << 4)

/**
 * Colors.
 */
#define	GWM_COLOR_SELECTION			&gwmColorSelection
extern DDIColor gwmColorSelection;

/**
 * Button flags.
 */
#define	GWM_BUTTON_DISABLED			(1 << 0)

/**
 * Text field flags.
 */
#define	GWM_TXT_DISABLED			(1 << 1)
#define	GWM_TXT_MASKED				(1 << 2)

/**
 * Those must match up with the Glidix humin interface (<glidix/humin.h>).
 */
#define	GWM_SC_MOUSE_LEFT			0x100
#define	GWM_SC_MOUSE_MIDDLE			0x101
#define	GWM_SC_MOUSE_RIGHT			0x102

/**
 * Message box icon types.
 */
#define	GWM_MBICON_NONE				0x0
#define	GWM_MBICON_INFO				0x1
#define	GWM_MBICON_QUEST			0x2
#define	GWM_MBICON_WARN				0x3
#define	GWM_MBICON_ERROR			0x4

/**
 * Message box button sets.
 */
#define	GWM_MBUT_OK				0x00
#define	GWM_MBUT_YESNO				0x10
#define	GWM_MBUT_OKCANCEL			0x20
#define	GWM_MBUT_YESNOCANCEL			0x30

/**
 * Key codes.
 */
#define	GWM_KC_LEFT				0x100
#define	GWM_KC_RIGHT				0x101
#define	GWM_KC_UP				0x102
#define	GWM_KC_DOWN				0x103
#define	GWM_KC_CTRL				0x104
#define	GWM_KC_SHIFT				0x105

/**
 * Cursor types.
 */
#define	GWM_CURSOR_NORMAL			0
#define	GWM_CURSOR_TEXT				1
#define	GWM_CURSOR_COUNT			2

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
#define	GWM_EVENT_ENTER				4
#define	GWM_EVENT_MOTION			5
#define	GWM_EVENT_LEAVE				6
#define	GWM_EVENT_FOCUS_IN			7
#define	GWM_EVENT_FOCUS_OUT			8
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
#define	GWM_CMD_SCREEN_SIZE			4
#define	GWM_CMD_SET_FLAGS			5
#define	GWM_CMD_SET_CURSOR			6
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
	
	struct
	{
		int				cmd;	// GWM_CMD_SCREEN_SIZE
		uint64_t			seq;
	} screenSize;
	
	struct
	{
		int				cmd;	// GWM_CMD_SET_FLAGS
		int				flags;
		uint64_t			seq;
		uint64_t			win;
	} setFlags;
	
	struct
	{
		int				cmd;	// GWM_CMD_SET_CURSOR
		int				cursor;
		uint64_t			seq;
		uint64_t			win;
	} setCursor;
} GWMCommand;

/**
 * Messages sent by the window manager to clients.
 */
#define GWM_MSG_CREATE_WINDOW_RESP		0
#define	GWM_MSG_EVENT				1
#define	GWM_MSG_CLEAR_WINDOW_RESP		2
#define	GWM_MSG_SCREEN_SIZE_RESP		3
#define	GWM_MSG_SET_FLAGS_RESP			4
#define	GWM_MSG_SET_CURSOR_RESP			5
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
	
	struct
	{
		int				type;	// GWM_MSG_SCREEN_SIZE_RESP
		uint64_t			seq;
		int				width;
		int				height;
	} screenSizeResp;
	
	struct
	{
		int				type;	// GWM_MSG_SET_FLAGS_RESP
		uint64_t			seq;
		int				status;
	} setFlagsResp;
	
	struct
	{
		int				type;	// GWM_MSG_SET_CURSOR_RESP
		uint64_t			seq;
		int				status;
	} setCursorResp;
} GWMMessage;

struct GWMWindow_;

/**
 * Event handler function type.
 * Return 0 when the applications should continue running; return -1 if the loop shall
 * terminate.
 */
typedef int (*GWMEventHandler)(GWMEvent *ev, struct GWMWindow_ *win);

/**
 * Those structures form lists of event handlers.
 */
typedef struct GWMHandlerInfo_
{
	struct GWMWindow_			*win;
	GWMEventHandler				callback;
	struct GWMHandlerInfo_			*prev;
	struct GWMHandlerInfo_			*next;
} GWMHandlerInfo;

/**
 * Describes a window on the application side.
 */
typedef struct GWMWindow_
{
	uint64_t				id;
	uint64_t				shmemAddr;
	uint64_t				shmemSize;
	DDISurface*				canvas;
	GWMHandlerInfo				*handlerInfo;
	void					*data;
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

/**
 * Set an event handler for the given window, that will be called from gwmMainLoop().
 */
void gwmSetEventHandler(GWMWindow *win, GWMEventHandler handler);

/**
 * Call this function if your event handler receives an event it does not respond to,
 * or to simply trigger the default handler.
 */
int gwmDefaultHandler(GWMEvent *ev, GWMWindow *win);

/**
 * Starts the main loop. Returns after an event handler requests an exit by returning -1.
 * You must use this when using the widget toolkit.
 */
void gwmMainLoop();

/**
 * Creates a new button in the specified window.
 */
GWMWindow* gwmCreateButton(GWMWindow *parent, const char *text, int x, int y, int width, int flags);

/**
 * Destroys a button.
 */
void gwmDestroyButton(GWMWindow *button);

/**
 * Set the callback function for a button. Return value is the same as for event handlers.
 */
typedef int (*GWMButtonCallback)(void *param);
void gwmSetButtonCallback(GWMWindow *button, GWMButtonCallback callback, void *param);

/**
 * Display a message box.
 */
int gwmMessageBox(GWMWindow *parent, const char *caption, const char *text, int flags);

/**
 * Get the width and height of the screen.
 */
void gwmScreenSize(int *width, int *height);

/**
 * Set window flags. This can also be used to make a window focused.
 */
int gwmSetWindowFlags(GWMWindow *win, int flags);

/**
 * Creates a new text field in the specified window.
 */
GWMWindow *gwmCreateTextField(GWMWindow *parent, const char *text, int x, int y, int width, int flags);

/**
 * Destroys a text field.
 */
void gwmDestroyTextField(GWMWindow *txt);

/**
 * Gets the current number of characters in a text field. You may use it to get the full text by
 * calling gwmReadTextField().
 */
size_t gwmGetTextFieldSize(GWMWindow *field);

/**
 * Reads from the specified position, up to and excluding the specified end position, into the
 * specified buffer. Returns the number of characters that were actually read.
 * Note that this also adds a terminator to the end of the string! So the size of the buffer
 * must be 1 larger than the number of charaters to store.
 */
size_t gwmReadTextField(GWMWindow *field, char *buffer, off_t startPos, off_t endPos);

/**
 * Sets which cursor should be used by a window. The cursor is one of the GWM_CURSOR_* macros.
 * Returns 0 on success, -1 on error.
 */
int gwmSetWindowCursor(GWMWindow *win, int cursor);

#endif
