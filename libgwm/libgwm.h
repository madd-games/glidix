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

#ifndef GUI_H_
#define GUI_H_

#include <stdint.h>
#include <libddi.h>
#include <unistd.h>

/**
 * Window flags.
 */
#define	GWM_WINDOW_HIDDEN			(1 << 0)
#define	GWM_WINDOW_NODECORATE			(1 << 1)
#define	GWM_WINDOW_MKFOCUSED			(1 << 2)
#define	GWM_WINDOW_NOTASKBAR			(1 << 3)
#define	GWM_WINDOW_NOSYSMENU			(1 << 4)
#define	GWM_WINDOW_RESIZEABLE			(1 << 5)
#define	GWM_WINDOW_NOICON			(1 << 6)

/**
 * Error codes.
 */
#define	GWM_ERR_NOWND				1		/* window doesn't exist */
#define	GWM_ERR_NOSURF				2		/* cannot open shared surface */
#define	GWM_ERR_INVAL				3		/* invalid value (e.g. already-used ID) */
#define	GWM_ERR_NOENT				4		/* name does not exist */

/**
 * Colors.
 */
#define	GWM_COLOR_SELECTION			&gwmColorSelection
extern DDIColor gwmColorSelection;
#define	GWM_COLOR_BACKGROUND			&gwmBackColor
extern DDIColor gwmBackColor;

/**
 * Unspecified window position.
 */
#define	GWM_POS_UNSPEC				((unsigned int)-1)

/**
 * Maximum timespan between 2 clicks that allows it to be considered a "double-click".
 */
#define	GWM_DOUBLECLICK_TIMEOUT			(1*CLOCKS_PER_SEC)

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
 * Checkbox flags.
 */
#define	GWM_CB_DISABLED				(1 << 0)

/**
 * Checkbox states.
 */
#define	GWM_CB_OFF				0
#define	GWM_CB_ON				1
#define	GWM_CB_TRI				2

/**
 * Radio button flags.
 */
#define	GWM_RADIO_DISABLED			(1 << 0)

/**
 * Scrollbar flags. Note that GWM_SCROLLBAR_HORIZ and GWM_SCROLLBAR_VERT are to be
 * considered mutually exclusive, and GWM_SCROLLBAR_VERT is the default.
 */
#define	GWM_SCROLLBAR_VERT			0
#define	GWM_SCROLLBAR_HORIZ			(1 << 0)
#define	GWM_SCROLLBAR_DISABLED			(1 << 1)

/**
 * Those must match up with the Glidix humin interface (<glidix/humin.h>).
 * TODO: remove scancode stuff, we use keycodes only now!
 */
#define	GWM_SC_MOUSE_LEFT			0x100
#define	GWM_SC_MOUSE_MIDDLE			0x101
#define	GWM_SC_MOUSE_RIGHT			0x102

/**
 * Message box icon types. Bottom 4 bits of the 'flags' parameter.
 */
#define	GWM_MBICON_NONE				0x0
#define	GWM_MBICON_INFO				0x1
#define	GWM_MBICON_QUEST			0x2
#define	GWM_MBICON_WARN				0x3
#define	GWM_MBICON_ERROR			0x4

/**
 * Message box button sets. Bits 4-7 of the 'flags' parameter.
 */
#define	GWM_MBUT_OK				0x00
#define	GWM_MBUT_YESNO				0x10
#define	GWM_MBUT_OKCANCEL			0x20
#define	GWM_MBUT_YESNOCANCEL			0x30

/**
 * Menu entry flags.
 */
#define	GWM_ME_SEPARATOR			(1 << 0)		/* draw separator below */

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
#define	GWM_CURSOR_HAND				2
#define	GWM_CURSOR_COUNT			3

/**
 * Slider flags.
 */
#define	GWM_SLIDER_VERT				0
#define	GWM_SLIDER_HORIZ			(1 << 0)
#define	GWM_SLIDER_DISABLED			(1 << 1)

/**
 * Combo box flags; make them consistent with text field flags!
 */
#define	GWM_COMBO_DISABLED			(1 << 1)
#define	GWM_COMBO_MASKED			(1 << 2)

/**
 * Option menu flags.
 */
#define	GWM_OPTMENU_DISABLED			(1 << 0)

/**
 * Text area flags.
 */
#define	GWM_TEXTAREA_DISABLED			(1 << 0)

/**
 * Stacking options (above or below).
 */
#define	GWM_STACK_ABOVE				0
#define	GWM_STACK_BELOW				1
#define	GWM_STACK_REMOVE			2

/**
 * Icon sizes.
 */
#define	GWM_FICON_SMALL				0
#define	GWM_FICON_LARGE				1

/**
 * Data types for theme descriptions.
 */
#define	GWM_TYPE_SURFACE			1
#define	GWM_TYPE_COLOR				2

/**
 * Window manager information structure, located in the shared file /run/gwminfo.
 * Contains information shared by all applications, such as the desktop background
 * surface, and theme information.
 */
typedef struct
{
	/**
	 * Window caption surface.
	 */
	uint32_t				imgWinCap;
	
	/**
	 * Window buttons surface.
	 */
	uint32_t				imgWinButtons;
	
	/**
	 * ID of the background surface.
	 */
	uint32_t				backgroundID;
	
	/**
	 * Color of active/inactive windows.
	 */
	DDIColor				colWinActive;
	DDIColor				colWinInactive;
} GWMInfo;

/**
 * A radio group. Specifies the currently-selected value from a list of radio buttons.
 * Also lists the buttons for redraw purposes.
 */
struct GWMRadioData_;
typedef struct
{
	int					value;
	struct GWMRadioData_*			first;
	struct GWMRadioData_*			last;
} GWMRadioGroup;

/**
 * Window parameter structure, as passed by clients to the window manager.
 */
typedef struct
{
	char					caption[256];
	char					iconName[256];
	unsigned int				flags;
	int					width;
	int					height;
	int					x;
	int					y;
} GWMWindowParams;

/**
 * Flags for @keymod.
 */
#define	GWM_KM_CTRL				(1 << 0)
#define	GWM_KM_SHIFT				(1 << 1)
#define	GWM_KM_ALT				(1 << 2)
#define	GWM_KM_CAPS_LOCK			(1 << 3)
#define	GWM_KM_NUM_LOCK				(1 << 4)
#define	GWM_KM_SCROLL_LOCK			(1 << 5)

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
#define	GWM_EVENT_DESKTOP_UPDATE		9
#define	GWM_EVENT_RESIZE_REQUEST		10
#define	GWM_EVENT_DOUBLECLICK			11
typedef struct
{
	int					type;
	uint64_t				win;		// window ID
	int					x;		// mouse position relative to window; or window
	int					y;		// position for GWM_EVENT_RESIZE_REQUEST
	int					scancode;	// hardware-specific key scancode
	int					keycode;	// hardware-independent key code
	int					keymod;		// bitwise-OR of active modifiers (GWM_KM_*)
	uint64_t				keychar;	// actual Unicode character entered by key, or zero
	unsigned int 				width;		// requested window dimensions (for GWM_EVENT_RESIZE_REQUEST)
	unsigned int				height;
} GWMEvent;

/**
 * Global window reference - uniquely identifies a window on the screen, which may belong
 * to any application. This is used for the "get window info" commands, by the system bar
 * for example.
 */
typedef struct
{
	uint64_t				id;
	int					pid;
	int					fd;
} GWMGlobWinRef;

/**
 * Commands understood by the window manager.
 */
#define	GWM_CMD_CREATE_WINDOW			0
#define	GWM_CMD_POST_DIRTY			1
#define	GWM_CMD_DESTROY_WINDOW			2
/* 3: unused */
#define	GWM_CMD_SCREEN_SIZE			4
#define	GWM_CMD_SET_FLAGS			5
#define	GWM_CMD_SET_CURSOR			6
#define	GWM_CMD_SET_ICON			7
#define	GWM_CMD_GET_FORMAT			8
#define	GWM_CMD_GET_WINDOW_LIST			9
#define	GWM_CMD_GET_WINDOW_PARAMS		10
#define	GWM_CMD_SET_LISTEN_WINDOW		11
#define	GWM_CMD_TOGGLE_WINDOW			12
#define	GWM_CMD_RESIZE				13
#define	GWM_CMD_MOVE				14
#define	GWM_CMD_REL_TO_ABS			15
#define	GWM_CMD_REDRAW_SCREEN			16
#define	GWM_CMD_SCREENSHOT_WINDOW		17
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
		uint32_t			surfID;	// client area surface ID
	} createWindow;
	
	struct
	{
		int				cmd;	// GWM_CMD_POST_DIRTY
		uint64_t			id;
		uint64_t			seq;
	} postDirty;
	
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
	
	struct
	{
		int				cmd;	// GWM_CMD_SET_ICON
		uint32_t			surfID;	// surface ID of icon surface
		uint64_t			seq;
		uint64_t			win;
	} setIcon;
	
	struct
	{
		int				cmd;	// GWM_CMD_GET_FORMAT
		uint64_t			seq;
	} getFormat;
	
	struct
	{
		int				cmd;	// GWM_CMD_GET_WINDOW_LIST
		uint64_t			seq;
	} getWindowList;
	
	struct
	{
		int				cmd;	// GWM_CMD_GET_WINDOW_PARAMS
		uint64_t			seq;
		GWMGlobWinRef			ref;
	} getWindowParams;
	
	struct
	{
		int				cmd;	// GWM_CMD_SET_LISTEN_WINDOW
		uint64_t			seq;
		uint64_t			win;
	} setListenWindow;
	
	struct
	{
		int				cmd;	// GWM_CMD_TOGGLE_WINDOW
		uint64_t			seq;
		GWMGlobWinRef			ref;
	} toggleWindow;
	
	struct
	{
		int				cmd;	// GWM_CMD_RESIZE
		uint64_t			seq;
		uint64_t			win;
		unsigned int			width;
		unsigned int			height;
	} resize;
	
	struct
	{
		int				cmd;	// GWM_CMD_MOVE
		uint64_t			seq;
		uint64_t			win;
		int				x;
		int				y;
	} move;
	
	struct
	{
		int				cmd;	// GWM_CMD_REL_TO_ABS
		uint64_t			seq;
		uint64_t			win;
		int				relX;
		int				relY;
	} relToAbs;
	
	struct
	{
		int				cmd;	// GWM_CMD_SCREENSHOT_WINDOW
		uint64_t			seq;
		uint64_t			id;
		GWMGlobWinRef			ref;	// of the window being screenshotted
	} screenshotWindow;
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
#define	GWM_MSG_SET_ICON_RESP			6
#define	GWM_MSG_GET_FORMAT_RESP			7
#define	GWM_MSG_GET_WINDOW_LIST_RESP		8
#define	GWM_MSG_GET_WINDOW_PARAMS_RESP		9
#define	GWM_MSG_TOGGLE_WINDOW_RESP		10
#define	GWM_MSG_RESIZE_RESP			11
#define	GWM_MSG_REL_TO_ABS_RESP			12
#define	GWM_MSG_SCREENSHOT_WINDOW_RESP		13
#define	GWM_MSG_POST_DIRTY_RESP			14
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
	
	struct
	{
		int				type;	// GWM_MSG_SET_ICON_RESP
		uint64_t			seq;
		int				status;
	} setIconResp;
	
	struct
	{
		int				type;	// GWM_MSG_GET_FORMAT_RESP
		uint64_t			seq;
		DDIPixelFormat			format;
	} getFormatResp;
	
	struct
	{
		int				type;	// GWM_MSG_GET_WINDOW_LIST_RESP
		uint64_t			seq;
		int				count;
		GWMGlobWinRef			focused;
		GWMGlobWinRef			wins[128];
	} getWindowListResp;
	
	struct
	{
		int				type;	// GWM_MSG_GET_WINDOW_PARAMS_RESP
		uint64_t			seq;
		int				status;
		GWMWindowParams			params;
	} getWindowParamsResp;
	
	struct
	{
		int				type;	// GWM_MSG_TOGGLE_WINDOW_RESP
		uint64_t			seq;
		int				status;
	} toggleWindowResp;
	
	struct
	{
		int				type;	// GWM_MSG_RESIZE_RESP
		uint64_t			seq;
		int				status;	// 0 = success, other = GWM error code
		unsigned int			width;
		unsigned int			height;
		uint32_t			clientID[2];	// TODO: remove me
	} resizeResp;
	
	struct
	{
		int				type;	// GWM_MSG_REL_TO_ABS_RESP
		uint64_t			seq;
		int				absX;
		int				absY;
	} relToAbsResp;
	
	struct
	{
		int				type;	// GWM_MSG_SCREENSHOT_WINDOW_RESP
		uint64_t			seq;
		int				status;	// 0 = success
		DDIPixelFormat			format;
		uint32_t			clientID[2];
		unsigned int			width;
		unsigned int			height;
	} screenshotWindowResp;
	
	struct
	{
		int				type;	// GWM_MSG_POST_DIRTY_RESP
		uint64_t			seq;
		int				status;
	} postDirtyResp;
} GWMMessage;

struct GWMWindow_;

/**
 * Event handler function type.
 * Return 0 when the application should continue running; return -1 if the loop shall
 * terminate.
 */
typedef int (*GWMEventHandler)(GWMEvent *ev, struct GWMWindow_ *win);

/**
 * These structures form lists of event handlers.
 */
typedef struct GWMHandlerInfo_
{
	struct GWMWindow_			*win;
	GWMEventHandler				callback;
	struct GWMHandlerInfo_			*prev;
	struct GWMHandlerInfo_			*next;
	
	/**
	 * 0 = handler not being executed
	 * 1 = handler being executed
	 * 2 = handler being executed and window was deleted.
	 */
	int					state;
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
	GWMHandlerInfo*				handlerInfo;
	void*					data;
	
	// location and time of last left click; used to detect double-clicks.
	int					lastClickX;
	int					lastClickY;
	clock_t					lastClickTime;
	
	// modal ID; 0 = main program, other values = modal dialogs
	uint64_t				modalID;
	
	/**
	 * Icon surface.
	 */
	DDISurface*				icon;
} GWMWindow;

/**
 * Menu entry callback; return -1 to terminate application, 0 to continue.
 */
typedef int (*GWMMenuCallback)(void *param);

/**
 * Menu close callback.
 */
typedef void (*GWMMenuCloseCallback)(void *param);

/**
 * Describes a menu entry.
 */
struct GWMMenu_;
typedef struct
{
	char*					label;		// on the heap!
	int					flags;
	GWMMenuCallback				callback;
	void*					param;		// callback parameter
	struct GWMMenu_*			submenu;
	DDISurface*				icon;		// or NULL for no icon
} GWMMenuEntry;

/**
 * Describes a menu. You create a menu with gwmCreateMenu(), open it using gwmOpenMenu(), close with gwmCloseMenu()
 * (or by the user clicking away from it). You can destroy it with gwmDestroyMenu().
 */
typedef struct GWMMenu_
{
	size_t					numEntries;
	GWMMenuEntry*				entries;
	GWMWindow*				win;
	DDISurface*				background;
	DDISurface*				overlay;
	GWMMenuCloseCallback			closeCallback;
	void*					closeParam;
	int					selectedSub;
	int					focused;
} GWMMenu;

/**
 * Tree node flags.
 */
#define	GWM_NODE_HASCHLD			(1 << 0)			/* has children */

/**
 * Tree node information.
 */
typedef struct
{
	/**
	 * Pointer to a buffer of size tePathSize, into which the entry's path shall be stored.
	 * This pointer is initialized by the TreeView widget.
	 */
	void*					niPath;
	
	/**
	 * Node flags; 0, or bitwise-OR of one or more of GWM_NODE_*
	 */
	int					niFlags;
	
	/**
	 * Pointer to an array of 'teNumCols' pointers, each specifying the value for a corresponding
	 * column. All entries are to be on the heap, and they are free()d by the TreeView widget.
	 * This pointer itself is initialized by the TreeView widget.
	 */
	void**					niValues;
	
	/**
	 * A 16x16 surface to use as an icon for the entry. This is initialized to NULL by the TreeView
	 * widget, indicating that no icon will be used by default. The TreeView widget WILL NOT call
	 * ddiDeleteSurface() on this.
	 */
	DDISurface*				niIcon;
} GWMNodeInfo;

/**
 * Describes a tree enumerator for use with a TreeView widget.
 */
typedef struct
{
	/**
	 * Size of this structure.
	 */
	size_t					teSize;
	
	/**
	 * The size of a 'path' used by this enumerator.
	 */
	size_t					tePathSize;
	
	/**
	 * Number of columns that this enumerator returns.
	 */
	size_t					teNumCols;
	
	/**
	 * Returns the caption of the column with the given index; the returned string shall
	 * be on the heap, and the TreeView widget will call free() on it.
	 */
	char* (*teGetColCaption)(int index);
	
	/**
	 * Open a node on the tree. Return NULL if that failed; a pointer to an opaque iterator structure
	 * otherwise.
	 */
	void* (*teOpenNode)(const void *path);
	
	/**
	 * Get the next entry of a node; return 0 on success or -1 if there are no more entries. If an entry
	 * does exist, the 'info' structure shall be filled in with information about the node. The 'node'
	 * argument is one previously returned by teOpenNode().
	 */
	int (*teGetNext)(void *node, GWMNodeInfo *info, size_t infoSize);
	
	/**
	 * Close the node previously opened by teOpenNode().
	 */
	void (*teCloseNode)(void *node);
} GWMTreeEnum;

extern GWMTreeEnum gwmTreeFilesystem;
extern char gwmFSRoot[PATH_MAX];
#define	GWM_TREE_FILESYSTEM (&gwmTreeFilesystem)
#define	GWM_FS_ROOT gwmFSRoot

/**
 * TreeView flags.
 */
#define	GWM_TV_NOHEADS				(1 << 0)

/**
 * File chooser modes.
 */
#define	GWM_FILE_OPEN				0
#define	GWM_FILE_SAVE				1

/**
 * Clipboard magic number.
 */
#define	GWM_CB_MAGIC				0xC1B0

/**
 * Clipboard data types.
 */
#define	GWM_CB_EMPTY				0
#define	GWM_CB_TEXT				1

/**
 * This data structure is placed at the beginnig of /run/clipboard to specify information about the clipboard
 * contents.
 */
typedef struct
{
	uint16_t				cbMagic;
	uint16_t				cbType;
	char					cbPad[60];		/* pad to 64 bytes */
} GWMClipboardHeader;

/**
 * Describes a tag.
 */
typedef struct
{
	int					flags;			// which fields are set
	DDIColor				fg;
	DDIColor				bg;
} GWMTag;
extern GWMTag gwmTagSelection;
#define	GWM_TAG_SELECTION			(&gwmTagSelection)

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
	int x, int y,
	int width, int height,
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
 * Tell the window manager that a window needs re-drawing.
 */
void gwmPostDirty(GWMWindow *win);

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
 * Set the width of a text field.
 */
void gwmResizeTextField(GWMWindow *field, int width);

/**
 * Change the text in a text field.
 */
void gwmWriteTextField(GWMWindow *field, const char *newText);

/**
 * Select the entire contents of the text field.
 */
void gwmTextFieldSelectAll(GWMWindow *field);

/**
 * Sets the callback for when the user pressed ENTER while typing in a text field.
 */
typedef int (*GWMTextFieldCallback)(void *param);
void gwmSetTextFieldAcceptCallback(GWMWindow *field, GWMTextFieldCallback callback, void *param);

/**
 * Sets which cursor should be used by a window. The cursor is one of the GWM_CURSOR_* macros.
 * Returns 0 on success, -1 on error.
 */
int gwmSetWindowCursor(GWMWindow *win, int cursor);

/**
 * Sets the icon of a window. The specified surface must be 16x16, in screen format; otherwise,
 * undefined behaviour occurs.
 */
int gwmSetWindowIcon(GWMWindow *win, DDISurface *icon);

/**
 * Returns the default font for use with DDI.
 */
DDIFont *gwmGetDefaultFont();

/**
 * Get the screen format.
 */
void gwmGetScreenFormat(DDIPixelFormat *format);

/**
 * Gets a list of top-level windows currently open on the desktop, as well as the currently-focused
 * window. Returns the amount of window references actually stored in the array. The array must be
 * large enough to store at least 128 window references. The pid of the focused window will be zero
 * if no window is focused. Either or both of the arguments may be NULL if you want to ignore the
 * corresponding information.
 */
int gwmGetDesktopWindows(GWMGlobWinRef *focused, GWMGlobWinRef *winlist);

/**
 * Gets window parameters for a global reference. Returns 0 on success, -1 on error.
 */
int gwmGetGlobWinParams(GWMGlobWinRef *ref, GWMWindowParams *pars);

/**
 * Toggle a window from unfocused to focused+visible, or between hidden and visible.
 */
void gwmToggleWindow(GWMGlobWinRef *ref);

/**
 * Mark a certain window as a "listening window". This window will receive the GWM_EVENT_DESKTOP_UPDATE
 * event when the desktop window list, or focused window, changed. Used by the sysbar for example.
 */
void gwmSetListenWindow(GWMWindow *win);

/**
 * Creates a new checkbox in the specified window.
 */
GWMWindow *gwmCreateCheckbox(GWMWindow *parent, int x, int y, int state, int flags);

/**
 * Destroys a checkbox.
 */
void gwmDestroyCheckbox(GWMWindow *checkbox);

/**
 * Set checkbox flags.
 */
void gwmSetCheckboxFlags(GWMWindow *checkbox, int flags);

/**
 * Return the current state of a checkbox (GWM_CB_OFF, GWM_CB_ON, or GWM_CB_TRI).
 */
int gwmGetCheckboxState(GWMWindow *checkbox);

/**
 * Set the state of a checkbox (GWM_CB_OFF, GWM_CB_ON or GWM_CB_TRI).
 */
void gwmSetCheckboxState(GWMWindow *checkbox, int state);

/**
 * Create a scrollbar.
 */
GWMWindow *gwmCreateScrollbar(GWMWindow *parent, int x, int y, int len, int viewOffset, int viewSize, int viewTotal, int flags);

/**
 * Destroy a scrollbar.
 */
void gwmDestroyScrollbar(GWMWindow *sbar);

/**
 * Set the update handler for a scrollbar. The handler's return value is interpreted the same way as an event
 * handler's return value.
 */
typedef int (*GWMScrollbarCallback)(void *param);
void gwmSetScrollbarCallback(GWMWindow *sbar, GWMScrollbarCallback callback, void *param);

/**
 * Get the scrollbar's current view offset.
 */
int gwmGetScrollbarOffset(GWMWindow *sbar);

/**
 * Set scrollbar parameters and redraw. Note: the orientation will NOT be changed by this function.
 */
void gwmSetScrollbarParams(GWMWindow *sbar, int viewOffset, int viewSize, int viewTotal, int flags);

/**
 * Change the length of a scrollbar.
 */
void gwmSetScrollbarLen(GWMWindow *sbar, int len);

/**
 * Change the size of a window. This frees, and hence invalidates, a previous return value from gwmGetWindowCanvas()!
 */
void gwmResizeWindow(GWMWindow *win, unsigned int width, unsigned int height);

/**
 * Change the position of a window.
 */
void gwmMoveWindow(GWMWindow *win, int x, int y);

/**
 * Convert the coordinates (relX, relY), relative to window 'win' (which may be NULL to indicate the desktop), and return
 * the absolute coordinates in (absX, absY).
 */
void gwmRelToAbs(GWMWindow *win, int relX, int relY, int *absX, int *absY);

/**
 * Creates a new menu.
 */
GWMMenu *gwmCreateMenu();

/**
 * Adds a new entry to a menu.
 */
void gwmMenuAddEntry(GWMMenu *menu, const char *label, GWMMenuCallback callback, void *param);

/**
 * Adds a separator to a menu.
 */
void gwmMenuAddSeparator(GWMMenu *menu);

/**
 * Adds a sub-menu to a menu.
 */
GWMMenu *gwmMenuAddSub(GWMMenu *menu, const char *label);

/**
 * Set the icon of the last entry in the menu. The icon shall be 16x16.
 */
void gwmMenuSetIcon(GWMMenu *menu, DDISurface *icon);

/**
 * Sets the close callback of a menu.
 */
void gwmMenuSetCloseCallback(GWMMenu *menu, GWMMenuCloseCallback callback, void *param);

/**
 * Opens the specified menu on top of the specified window at the specified position.
 */
void gwmOpenMenu(GWMMenu *menu, GWMWindow *win, int x, int y);

/**
 * Closes the specified menu.
 */
void gwmCloseMenu(GWMMenu *menu);

/**
 * Destroys a menu object.
 */
void gwmDestroyMenu(GWMMenu *menu);

/**
 * Creates a menu bar.
 */
GWMWindow *gwmCreateMenubar(GWMWindow *parent);

/**
 * Adjust a menu bar to its parent's new width. Call this whenever the parent is resized.
 */
void gwmMenubarAdjust(GWMWindow *menubar);

/**
 * Add a new menu to a menubar. The menubar takes ownership of the menu; you must not call gwmOpenMenu(), gwmCloseMenu()
 * or gwmDestroyMenu() on it (gwmDestroyMenu() will be automatically called when you call gwmDestroyMenubar()).
 */
void gwmMenubarAdd(GWMWindow *menubar, const char *label, GWMMenu *menu);

/**
 * Classify the character 'c' as either a whitespace (0) or other character (1).
 */
int gwmClassifyChar(char c);

/**
 * Create a new notebook widget.
 */
GWMWindow *gwmCreateNotebook(GWMWindow *parent, int x, int y, int width, int height, int flags);

/**
 * Returns the dimensions of the client area of a notebook tab.
 */
void gwmNotebookGetDisplaySize(GWMWindow *notebook, int *width, int *height);

/**
 * Adds a new tab to a notebook, with the specified label, and return its display window.
 */
GWMWindow *gwmNotebookAdd(GWMWindow *notebook, const char *label);

/**
 * Sets which tab (given as a zero-based index) is to be visible in the notebook.
 */
void gwmNotebookSetTab(GWMWindow *notebook, int index);

/**
 * Destroy a notebook widget.
 */
void gwmDestroyNotebook(GWMWindow *notebook);

/**
 * Put plain text in the clipboard.
 */
void gwmClipboardPutText(const char *text, size_t textSize);

/**
 * Attempt to get plain text from the clipboard. On success, returns a pointer to the buffer containing
 * the text, and sets *sizeOut to the size; you are responsible for later calling free() on the buffer
 * to release it. If no text is in the clipboard, returns NULL. A NUL terminator is placed at the end
 * of the buffer; the 'sizeOut' value does NOT include this NUL character!
 */
char* gwmClipboardGetText(size_t *sizeOut);

/**
 * Create a new TreeView widget, whose root displays the path 'root' in the specified 'tree'.
 */
GWMWindow *gwmCreateTreeView(GWMWindow *parent, int x, int y, int width, int height, GWMTreeEnum *tree, const void *root, int flags);

/**
 * Destroy a TreeView object.
 */
void gwmDestroyTreeView(GWMWindow *treeview);

/**
 * Set a TreeView double-click handler. The TreeView event handler will return whatever the callback returns.
 */
typedef int (*GWMTreeViewActivateCallback)(void *param);
void gwmTreeViewSetActivateCallback(GWMWindow *treeview, GWMTreeViewActivateCallback handler, void *param);

/**
 * Set a TreeView selection handler.
 */
typedef void (*GWMTreeViewSelectCallback)(void *param);
void gwmTreeViewSetSelectCallback(GWMWindow *treeview, GWMTreeViewSelectCallback handler, void *param);

/**
 * Store the currently-selected path on a TreeView in the given buffer. Returns 0 if something actually was
 * selected; otherwise returns -1 and the buffer is untouched.
 */
int gwmTreeViewGetSelection(GWMWindow *treeview, void *buffer);

/**
 * Change the tree in a TreeView and refresh.
 */
void gwmTreeViewUpdate(GWMWindow *treeview, const void *root);

/**
 * Create a modal dialog. You may place children in it, and then run it using gwmRunModal(). You can set an
 * event handler as for a normal window. A modal dialog closes when an event handler returns -1 (the application
 * does not close in this case). You may also use the "data" field in the window in whatever way you wish.
 */
GWMWindow* gwmCreateModal(const char *caption, int x, int y, int width, int height);

/**
 * Run a modal dialog. This function will return when an event handler within the modal dialog returns -1.
 */
void gwmRunModal(GWMWindow *modal, int flags);

/**
 * Displays a dialog asking the user to input a line of text. If the user clicks "Cancel", returns NULL.
 * Otherwise, returns the string that was typed in. The string is placed on the heap, so you must call
 * free() on it.
 */
char* gwmGetInput(const char *caption, const char *prompt, const char *initialText);

/**
 * Return a pointer to the GWM information structure.
 */
GWMInfo *gwmGetInfo();

/**
 * Tell the window manager to redraw the whole screen.
 */
void gwmRedrawScreen();

/**
 * Creates a radio group, with the specified initial value.
 */
GWMRadioGroup* gwmCreateRadioGroup(int value);

/**
 * Destroy a radio group. Note that you must first destroy all its component radio buttons!
 */
void gwmDestroyRadioGroup(GWMRadioGroup *group);

/**
 * Get the current value of a radio group, indicating the selected radio button.
 */
int gwmGetRadioGroupValue(GWMRadioGroup *group);

/**
 * Set the value of a radio group to the given value.
 */
void gwmSetRadioGroupValue(GWMRadioGroup *group, int value);

/**
 * Create a radio button, which will join the specified group, and when selected by the user, sets the value
 * of the radio group to 'value'.
 */
GWMWindow* gwmCreateRadioButton(GWMWindow *parent, int x, int y, GWMRadioGroup *group, int value, int flags);

/**
 * Destroy a radio button.
 */
void gwmDestroyRadioButton(GWMWindow *radio);

/**
 * Create a slider.
 */
GWMWindow* gwmCreateSlider(GWMWindow *parent, int x, int y, int len, int value, int max, int flags);

/**
 * Destroy a slider.
 */
void gwmDestroySlider(GWMWindow *slider);

/**
 * Set slider parameters. The orientation is ignored in 'flags'.
 */
void gwmSetSliderParams(GWMWindow *slider, int value, int max, int flags);

/**
 * Get the current value of the slider.
 */
int gwmGetSliderValue(GWMWindow *slider);

/**
 * Turn a window handle into a global window reference.
 */
void gwmGetGlobRef(GWMWindow *win, GWMGlobWinRef *ref);

/**
 * Take a screenshot of the specified window (must be top-level and not hidden). On success, returns a hidden
 * window handle where the currently-selected canvas contains a screenshot of the selected window. On error
 * (non-exiting or hidden window), returns NULL.
 */
GWMWindow *gwmScreenshotWindow(GWMGlobWinRef *ref);

/**
 * Create a combo box. A combo box contains a text field which the user may edit, and a dropdown menu which
 * allows the selection of some suggested options.
 */
GWMWindow* gwmCreateCombo(GWMWindow *parent, const char *text, int x, int y, int width, int flags);

/**
 * Destroy a combo box.
 */
void gwmDestroyCombo(GWMWindow *combo);

/**
 * Return the text field inside the combo box. This can be used to retrieve the text etc. Do NOT destroy the field
 * manually! It is owned by the combo box and is destroyed when gwmDestroyCombo() is called.
 */
GWMWindow* gwmGetComboTextField(GWMWindow *combo);

/**
 * Clear the list of options in a combo box.
 */
void gwmClearComboOptions(GWMWindow *combo);

/**
 * Add a new option to the list of combo box options. The string passed in is copied, so you may free the buffer after
 * use.
 */
void gwmAddComboOption(GWMWindow *combo, const char *opt);

/**
 * Create an option menu widget.
 */
GWMWindow* gwmCreateOptionMenu(GWMWindow *parent, uint64_t id, const char *text, int x, int y, int width, int flags);

/**
 * Destroy an option menu.
 */
void gwmDestroyOptionMenu(GWMWindow *optmenu);

/**
 * Clear an option menu.
 */
void gwmClearOptionMenu(GWMWindow *optmenu);

/**
 * Change the value in an option menu.
 */
void gwmSetOptionMenu(GWMWindow *optmenu, uint64_t id, const char *text);

/**
 * Add an option to an option menu.
 */
void gwmAddOptionMenu(GWMWindow *optmenu, uint64_t id, const char *text);

/**
 * Get the ID of the currently-selected option in the menu.
 */
uint64_t gwmReadOptionMenu(GWMWindow *optmenu);

/**
 * Create a frame widget with the specified caption.
 */
GWMWindow* gwmCreateFrame(GWMWindow *parent, const char *caption, int x, int y, int width, int height);

/**
 * Destroy a frame widget.
 */
void gwmDestroyFrame(GWMWindow *frame);

/**
 * Get the panel of the specified frame, into which children may be placed to put them in the frame.
 * The panel is owned by the frame; do not delete it. It will be deleted when the frame is deleted.
 */
GWMWindow* gwmGetFramePanel(GWMWindow *frame);

/**
 * Create a new text area tag.
 */
GWMTag* gwmCreateTag();

/**
 * Destroy a text area tag.
 */
void gwmDestroyTag(GWMTag *tag);

/**
 * Set the foreground color of a tag. Setting to NULL removes the foreground.
 */
void gwmSetTagForeground(GWMTag *tag, DDIColor *fg);

/**
 * Set the background color of a tag. Setting to NULL removed the background.
 */
void gwmSetTagBackground(GWMTag *tag, DDIColor *bg);

/**
 * Create a text area widget, initially containing no text.
 */
GWMWindow* gwmCreateTextArea(GWMWindow *parent, int x, int y, int width, int height, int flags);

/**
 * Destroy a text area widget.
 */
void gwmDestroyTextArea(GWMWindow *area);

/**
 * Append text to the end of a text area.
 */
void gwmAppendTextArea(GWMWindow *area, const char *text);

/**
 * Insert a tag at the given offset in the text area, with the specified length in bytes.
 */
void gwmTagTextArea(GWMWindow *area, GWMTag *tag, size_t pos, size_t len, int stacking);

/**
 * Insert text in a specified position in the text area.
 */
void gwmTextAreaInsert(GWMWindow *area, off_t pos, const char *text);

/**
 * Remove text from a text area.
 */
void gwmTextAreaErase(GWMWindow *area, off_t pos, size_t len);

/**
 * Return the first range of a tag occuring after, or on, the specified offset, in the specified pointers.
 * The "size" will be 0 if none was found.
 */
void gwmGetTextAreaTagRange(GWMWindow *area, GWMTag *tag, off_t after, off_t *outPos, size_t *outLen);

/**
 * Change the default style in a text area (and redraw).
 */
void gwmSetTextAreaStyle(GWMWindow *area, DDIFont *font, DDIColor *background, DDIColor *txtbg, DDIColor *txtfg);

/**
 * Change the text area flags.
 */
void gwmSetTextAreaFlags(GWMWindow *area, int flags);

/**
 * Get the size of text in a text area.
 */
size_t gwmGetTextAreaLen(GWMWindow *area);

/**
 * Read text from the text area and return it into the specified buffer. NOTE: A NUL character is added at the end!
 */
void gwmReadTextArea(GWMWindow *area, off_t pos, size_t len, char *buffer);

/**
 * Set the callback for when text in a textarea changes.
 */
typedef void (*GWMTextAreaUpdateCallback)(void *param);
void gwmSetTextAreaUpdateCallback(GWMWindow *area, GWMTextAreaUpdateCallback cb, void *param);

/**
 * Create a file chooser dialog. 'mode' is either GWM_FILE_OPEN or GWM_FILE_SAVE. You can set other settings then call
 * gwmRunFileChooser() to run it.
 */
GWMWindow* gwmCreateFileChooser(GWMWindow *parent, const char *caption, int mode);

/**
 * Run a file chooser dialog. If the user clicks Cancel, NULL is returned; otherwise, the absolute path of the selected
 * file is returned, as a string on the heap; you must call free() on it later.
 *
 * NOTE: The file chooser is destroyed after running!
 */
char* gwmRunFileChooser(GWMWindow *fc);

/**
 * Get an file icon with the given name, in the given size (GWM_FICON_SMALL being 16x16 and GWM_FICON_LARGE being 64x64).
 * Always returns something; it may be a dummy icon.
 */
DDISurface* gwmGetFileIcon(const char *iconName, int size);

/**
 * Perform a global theme initialization. Only root can do this, it does not require gwmInit() to be called,
 * and should only happen once, by the gwmserver. Returns 0 on success, or -1 on error; in which case it also
 * prints an error message.
 */
int gwmGlobalThemeInit(DDIPixelFormat *format);

/**
 * Get a theme element. On success, returns a pointer whose type is dependent on the element type:
 *
 *	GWM_TYPE_SURFACE - A DDISurface* representing the shared surface.
 *	GWM_TYPE_COLOR - A DDIColor* pointer, directly into the shared area.
 *
 * On error, NULL is returned and 'errOut' is set to a GWM error code (GWM_ERR_*) if it's not NULL:
 *
 *	GWM_ERR_INVAL - Wrong type specified.
 *	GWM_ERR_NOENT - No such theme property.
 */
void* gwmGetThemeProp(const char *name, int type, int *errOut);

#endif
