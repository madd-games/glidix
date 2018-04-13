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
#define	GWM_ERR_NORC				5		/* not enough resources */

/**
 * Event handler return statuses.
 */
#define	GWM_EVSTATUS_OK				0
#define	GWM_EVSTATUS_BREAK			-1
#define	GWM_EVSTATUS_CONT			-2
#define	GWM_EVSTATUS_DEFAULT			-3

/**
 * True and false. Kinda makes stuff prettier.
 */
#define	GWM_TRUE				(1 == 1)
#define	GWM_FALSE				(1 == 0)

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
 * Message box icon types. Bottom 4 bits of the 'flags' parameter.
 */
#define	GWM_MBICON_NONE				0x0
#define	GWM_MBICON_INFO				0x1
#define	GWM_MBICON_QUEST			0x2
#define	GWM_MBICON_WARN				0x3
#define	GWM_MBICON_ERROR			0x4
#define	GWM_MBICON_SUCCESS			0x5

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
 * Key codes (those must match with <glidix/humin.h>).
 */
#define	GWM_KC_ESC				0x01B		/* escape (\e) */
#define	GWM_KC_BACKTICK				0x060		/* backtick (`) key */
#define	GWM_KC_1				0x031		/* number keys: */
#define	GWM_KC_2				0x032
#define	GWM_KC_3				0x033
#define	GWM_KC_4				0x034
#define	GWM_KC_5				0x035
#define	GWM_KC_6				0x036
#define	GWM_KC_7				0x037
#define	GWM_KC_8				0x038
#define	GWM_KC_9				0x039
#define	GWM_KC_0				0x030
#define	GWM_KC_HYPHEN				0x02D		/* hyphen/underscore key */
#define	GWM_KC_EQUALS				0x03D		/* equals/plus key */
#define	GWM_KC_BACKSPACE			0x008		/* backspace (\b) */
#define	GWM_KC_TAB				0x009		/* tab (\t) */
#define	GWM_KC_Q				0x071
#define	GWM_KC_W				0x077
#define	GWM_KC_E				0x065
#define	GWM_KC_R				0x072
#define	GWM_KC_T				0x074
#define	GWM_KC_Y				0x079
#define	GWM_KC_U				0x075
#define	GWM_KC_I				0x069
#define	GWM_KC_O				0x06F
#define	GWM_KC_P				0x070
#define	GWM_KC_SQB_LEFT				0x05B		/* [{ key */
#define	GWM_KC_SQB_RIGHT			0x05D		/* ]} key */
#define	GWM_KC_RETURN				0x00D		/* carriage return (\r) */
#define	GWM_KC_ENTER				GWM_KC_RETURN	/* alias */
#define	GWM_KC_A				0x061
#define	GWM_KC_S				0x073
#define	GWM_KC_D				0x064
#define	GWM_KC_F				0x066
#define	GWM_KC_G				0x067
#define	GWM_KC_H				0x068
#define	GWM_KC_J				0x06A
#define	GWM_KC_K				0x06B
#define	GWM_KC_L				0x06C
#define	GWM_KC_SEMICOLON			0x03B		/* ;: key */
#define	GWM_KC_QUOTE				0x027		/* '" key */
#define	GWM_KC_BACKSLASH			0x05C		/* \| key */
#define	GWM_KC_ARROWS				0x03C		/* |\ or <> key (left of Z or left of RETURN) */
#define	GWM_KC_Z				0x07A
#define	GWM_KC_X				0x078
#define	GWM_KC_C				0x063
#define	GWM_KC_V				0x076
#define	GWM_KC_B				0x062
#define	GWM_KC_N				0x06E
#define	GWM_KC_M				0x06D
#define	GWM_KC_COMMA				0x02C		/* ,< key */
#define	GWM_KC_PERIOD				0x02E		/* .> key */
#define	GWM_KC_SLASH				0x02F		/* /? key */
#define	GWM_KC_SPACE				0x020		/* space bar */
#define	GWM_KC_LEFT				0x100		/* left arrow key */
#define	GWM_KC_RIGHT				0x101		/* right arrow key */
#define	GWM_KC_UP				0x102		/* up arrow key */
#define	GWM_KC_DOWN				0x103		/* down arrow key */
#define	GWM_KC_LCTRL				0x104		/* left ctrl */
#define	GWM_KC_LSHIFT				0x105		/* left shift */
#define	GWM_KC_LALT				0x106		/* left alt */
#define	GWM_KC_RCTRL				0x107		/* right ctrl */
#define	GWM_KC_RSHIFT				0x108		/* right shift */
#define	GWM_KC_RALT				0x109		/* right alt, or alt gr */
#define	GWM_KC_F1				0x10A
#define	GWM_KC_F2				0x10B
#define	GWM_KC_F3				0x10C
#define	GWM_KC_F4				0x10D
#define	GWM_KC_F5				0x10E
#define	GWM_KC_F6				0x10F
#define	GWM_KC_F7				0x110
#define	GWM_KC_F8				0x111
#define	GWM_KC_F9				0x112
#define	GWM_KC_F10				0x113
#define	GWM_KC_F11				0x114
#define	GWM_KC_F12				0x115
#define	GWM_KC_PRINT_SCREEN			0x116		/* print screen / sysrq */
#define	GWM_KC_SCROLL_LOCK			0x117
#define	GWM_KC_PAUSE				0x118
#define	GWM_KC_INSERT				0x119
#define	GWM_KC_HOME				0x11A
#define	GWM_KC_PAGE_UP				0x11B
#define	GWM_KC_NUM_LOCK				0x11C
#define	GWM_KC_NUMPAD_SLASH			0x11D
#define	GWM_KC_NUMPAD_ASTERISK			0x11E
#define	GWM_KC_NUMPAD_HYPHEN			0x11F
#define	GWM_KC_DELETE				0x120
#define	GWM_KC_END				0x121
#define	GWM_KC_PAGE_DOWN			0x122
#define	GWM_KC_NUMPAD_7				0x123
#define	GWM_KC_NUMPAD_8				0x124
#define	GWM_KC_NUMPAD_9				0x125
#define	GWM_KC_NUMPAD_PLUS			0x126
#define	GWM_KC_CAPS_LOCK			0x127
#define	GWM_KC_NUMPAD_4				0x128
#define	GWM_KC_NUMPAD_5				0x129
#define	GWM_KC_NUMPAD_6				0x12A
#define	GWM_KC_NUMPAD_1				0x12B
#define	GWM_KC_NUMPAD_2				0x12C
#define	GWM_KC_NUMPAD_3				0x12D
#define	GWM_KC_NUMPAD_RETURN			0x13E
#define	GWM_KC_NUMPAD_ENTER			GWM_KC_NUMPAD_RETURN
#define	GWM_KC_LSUPER				0x13F		/* left "super" key */
#define	GWM_KC_RSUPER				0x140		/* right "super" key */
#define	GWM_KC_CONTEXT				0x141		/* context menu key */
#define	GWM_KC_NUMPAD_0				0x142
#define	GWM_KC_NUMPAD_DEL			0x143		/* numpad delete key */
#define	GWM_KC_MOUSE_LEFT			0x144		/* left mouse button */
#define	GWM_KC_MOUSE_MIDDLE			0x145		/* middle mouse button */
#define	GWM_KC_MOUSE_RIGHT			0x146		/* right mouse button */

/**
 * Cursor types.
 */
#define	GWM_CURSOR_NORMAL			0
#define	GWM_CURSOR_TEXT				1
#define	GWM_CURSOR_HAND				2
#define	GWM_CURSOR_SE_RESIZE			3
#define	GWM_CURSOR_COUNT			4		/* number of cursors */

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
#define	GWM_TYPE_INT				3

/**
 * Some nice typedefs.
 */
typedef	int					GWMbool;
typedef	int					GWMenum;

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
	
	/**
	 * "gui-shutdown" graphic. It is a 320x200 image where:
	 * The rectangle (74, 68) to (138, 132) is the "reboot" button.
	 * The rectangle (180, 68) to (244, 132) is the "shutdown" button.
	 * (Hence both 64x64)
	 */
	uint32_t				imgShutdown;
	
	/**
	 * Message box icons surface. They are layed out horizontally, each 32x32.
	 * From left to right: info, question, warning, error, success.
	 */
	uint32_t				imgMessageIcons;
	
	/**
	 * Button graphics. 4 sprites on top of each other, each 17x30. In order from top
	 * to bottom: normal, hovered, clicked, disabled.
	 *
	 * Each button is rendered with the leftmost 8 pixels from the sprite on the left,
	 * the middle column of pixels repeated until the size is correct, and the rightmost
	 * 8 pixels from the sprite.
	 */
	uint32_t				imgButton;
	
	/**
	 * Checkbox graphics. It's a grid of 20x20 cells. The rows are: normal, hovered, clicked,
	 * disabled. The columns are: unchcked, checked, mixed/tristate.
	 */
	uint32_t				imgCheckbox;
	
	/**
	 * Radio button graphics. It's a grid of 20x20 cells. The rows are: normal, hovered, clicked,
	 * disabled. The columns are: unselected, selected.
	 */
	uint32_t				imgRadioButton;
	
	/**
	 * System bar gradient image. This image must have a width of 1 pixel and height of 40 pixels.
	 * It is spread horizontally across the system bar.
	 */
	uint32_t				imgSysbar;
	
	/**
	 * System bar menu button image. This must be a 32x32 image.
	 */
	uint32_t				imgSysbarMenu;
	
	/**
	 * Task button images on the system bar. It's 5 sprites, horizontally, each 32x32.
	 * From left to right: focused window, alerted, hovered, clicked, normal.
	 */
	uint32_t				imgTaskButton;
	
	/**
	 * Option menu image.
	 */
	uint32_t				imgOptionMenu;
	
	/**
	 * Slider image (20x20).
	 */
	uint32_t				imgSlider;
	
	/**
	 * Color of the active portion of a slider.
	 */
	DDIColor				colSliderActive;
	
	/**
	 * Color of the inactive portion of a slider.
	 */
	DDIColor				colSliderInactive;
	
	/**
	 * Color of the disabled portion of a slider.
	 */
	DDIColor				colSliderDisabled;
	
	/**
	 * Background color of a scrollbar.
	 */
	DDIColor				colScrollbarBg;
	
	/**
	 * Foreground color of a scrollbar.
	 */
	DDIColor				colScrollbarFg;
	
	/**
	 * Disabled color of a scrollbar.
	 */
	DDIColor				colScrollbarDisabled;
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
 * Flags for the box layout.
 */
#define	GWM_BOX_HORIZONTAL			0		/* horizontal by default */
#define	GWM_BOX_VERTICAL			(1 << 0)	/* vertical with this flag */
#define	GWM_BOX_INCLUDE_HIDDEN			(1 << 1)	/* include hidden children in calculation */

/**
 * Flags for a box layout child.
 */
#define	GWM_BOX_LEFT				(1 << 0)	/* border on left side */
#define	GWM_BOX_RIGHT				(1 << 1)	/* border on right side */
#define	GWM_BOX_UP				(1 << 2)	/* border on up side */
#define	GWM_BOX_DOWN				(1 << 3)	/* border on down side */
#define	GWM_BOX_FILL				(1 << 4)	/* fill allocated space on parallel axis */

#define	GWM_BOX_ALL				0xF		/* border in all directions */

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
 * Symbols. Used to mark widgets with a common function. The symbol 0 is reserved (GWM_SYM_NONE).
 * When bit 24 is set, it indicates a user-defined symbol. So all user-defined symbols must use
 * symbols larger than or equal to GWM_SYM_USER.
 *
 * When bit 25 is set, it indicates temporary symbols, generated using gwmGenerateSymbol().
 */
#define	GWM_SYM_USER				(1 << 24)
#define	GWM_SYM_GEN				(1 << 25)

/**
 * Built-in symbols.
 */
#define	GWM_SYM_NONE				0
#define	GWM_SYM_OK				1
#define	GWM_SYM_CANCEL				2
#define	GWM_SYM_YES				3
#define	GWM_SYM_NO				4
#define	GWM_SYM_EXIT				5

/**
 * Starting numbers for event classes.
 * When bit 24 is set, the event is cascading.
 * When bit 25 is set, the event is user-defined (otherwise libgwm built-in).
 */
#define	GWM_EVENT_CASCADING			(1 << 24)
#define	GWM_EVENT_USER				(1 << 25)

/**
 * Built-in events.
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
#define	GWM_EVENT_RETHEME			12

/**
 * Cascading events.
 */
#define	GWM_EVENT_COMMAND			(GWM_EVENT_CASCADING + 1)
#define	GWM_EVENT_TOGGLED			(GWM_EVENT_CASCADING + 2)
#define	GWM_EVENT_VALUE_CHANGED			(GWM_EVENT_CASCADING + 3)

/**
 * General event structure.
 * NOTE: This structure must not be edited, for binary compatibility reasons.
 * It may sometimes be cast to other types, such as GWMCommandEvent, depending on
 * the 'type' field.
 */
typedef struct
{
	int					type;
	uint64_t				win;		// window ID
	int					x;		// {mouse position relative to window; or window
	int					y;		// {position for GWM_EVENT_RESIZE_REQUEST
	int					scancode;	// hardware-specific key scancode
	int					keycode;	// hardware-independent key code
	int					keymod;		// bitwise-OR of active modifiers (GWM_KM_*)
	uint64_t				keychar;	// actual Unicode character entered by key, or zero
	unsigned int 				width;		// requested window dimensions (for GWM_EVENT_RESIZE_REQUEST)
	unsigned int				height;
} GWMEvent;

/**
 * Command event structure. Aliased from GWMEvent when 'type' is GWM_EVENT_COMMAND.
 */
typedef struct
{
	GWMEvent				header;
	int					symbol;		// command symbol
	void*					data;		// associated data if applicable
} GWMCommandEvent;

/**
 * Global window reference - uniquely identifies a window on the screen, which may belong
 * to any application. This is used for the "get window info" commands, by the system bar
 * for example.
 */
typedef struct
{
	uint64_t				id;
	int					fd;
} GWMGlobWinRef;

/**
 * Flags for the 'which' parameter for the "atomic config" command.
 */
#define	GWM_AC_X				(1 << 0)
#define	GWM_AC_Y				(1 << 1)
#define	GWM_AC_WIDTH				(1 << 2)
#define	GWM_AC_HEIGHT				(1 << 3)
#define	GWM_AC_SCROLL_X				(1 << 4)
#define	GWM_AC_SCROLL_Y				(1 << 5)
#define	GWM_AC_CANVAS				(1 << 6)
#define	GWM_AC_CAPTION				(1 << 7)

/**
 * Commands understood by the window manager.
 */
#define	GWM_CMD_CREATE_WINDOW			0
#define	GWM_CMD_POST_DIRTY			1
#define	GWM_CMD_DESTROY_WINDOW			2
#define	GWM_CMD_RETHEME				3
#define	GWM_CMD_SCREEN_SIZE			4
#define	GWM_CMD_SET_FLAGS			5
#define	GWM_CMD_SET_CURSOR			6
#define	GWM_CMD_SET_ICON			7
#define	GWM_CMD_GET_FORMAT			8
#define	GWM_CMD_GET_WINDOW_LIST			9
#define	GWM_CMD_GET_WINDOW_PARAMS		10
#define	GWM_CMD_SET_LISTEN_WINDOW		11
#define	GWM_CMD_TOGGLE_WINDOW			12
#define	GWM_CMD_GET_GLOB_REF			13
#define	GWM_CMD_ATOMIC_CONFIG			14
#define	GWM_CMD_REL_TO_ABS			15
#define	GWM_CMD_REDRAW_SCREEN			16
#define	GWM_CMD_SCREENSHOT_WINDOW		17
#define	GWM_CMD_GET_GLOB_ICON			18
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
		int				cmd;	// GWM_CMD_GET_GLOB_REF
		uint64_t			seq;
		uint64_t			win;
	} getGlobRef;
	
	struct
	{
		int				cmd;	// GWM_CMD_ATOMIC_CONFIG
		uint64_t			seq;
		uint64_t			win;
		int				which;	// bitset of which properties to set
		int				x;
		int				y;
		int				width;
		int				height;
		int				scrollX;
		int				scrollY;
		uint32_t			canvasID;
		char				caption[256];
	} atomicConfig;
	
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
		uint32_t			surfID;	// surface to screenshot to
		GWMGlobWinRef			ref;	// of the window being screenshotted
	} screenshotWindow;
	
	struct
	{
		int				cmd;	// GWM_CMD_GET_GLOB_ICON
		uint32_t			seq;
		GWMGlobWinRef			ref;
	} getGlobIcon;
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
#define	GWM_MSG_GET_GLOB_REF_RESP		13
#define	GWM_MSG_REL_TO_ABS_RESP			12
#define	GWM_MSG_SCREENSHOT_WINDOW_RESP		13
#define	GWM_MSG_POST_DIRTY_RESP			14
#define	GWM_MSG_GET_GLOB_ICON_RESP		15
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
		int				type;	// GWM_MSG_GET_GLOB_REF_RESP
		uint64_t			seq;
		GWMGlobWinRef			ref;
	} getGlobRefResp;
	
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
		int				width;
		int				height;
	} screenshotWindowResp;
	
	struct
	{
		int				type;	// GWM_MSG_POST_DIRTY_RESP
		uint64_t			seq;
		int				status;
	} postDirtyResp;
	
	struct
	{
		int				type;	// GWM_MSG_GET_GLOB_ICON_RESP
		uint64_t			seq;
		int				status;
		uint32_t			surfID;	// of the icon
	} getGlobIconResp;
} GWMMessage;

struct GWMWindow_;

/**
 * Event handler function type.
 * Must return one of the following:
 *	GWM_EVSTATUS_OK - event handled, stop calling other handlers.
 *	GWM_EVSTATUS_BREAK - break from the loop (modal or main loop)
 *	GWM_EVSTATUS_CONT - continue calling other handlers
 */
typedef int (*GWMEventHandler)(GWMEvent *ev, struct GWMWindow_ *win, void *context);

/**
 * These structures form lists of event handlers.
 */
typedef struct GWMHandlerInfo_
{
	struct GWMWindow_			*win;
	GWMEventHandler				callback;
	struct GWMHandlerInfo_			*prev;
	struct GWMHandlerInfo_			*next;
	void*					context;
	int					refcount;
} GWMHandlerInfo;

/**
 * Represents an abstract layout manager.
 */
typedef struct GWMLayout_ GWMLayout;
struct GWMLayout_
{
	/**
	 * Layout-specific data.
	 */
	void*					data;
	
	/**
	 * Get the minimum size allowable by the layout.
	 */
	void (*getMinSize)(GWMLayout *layout, int *width, int *height);
	
	/**
	 * Get the preferred size of the layout. This is the one that will be set by gwmFit().
	 */
	void (*getPrefSize)(GWMLayout *layout, int *width, int *height);
	
	/**
	 * Trigger the layout. Move the contents to the specified position, and give them the
	 * specified size. Also trigger the layout of any children.
	 */
	void (*run)(GWMLayout *layout, int x, int y, int width, int height);
};

/**
 * Describes a window on the application side.
 */
typedef struct GWMWindow_
{
	uint64_t				id;
	uint64_t				shmemAddr;
	uint64_t				shmemSize;
	DDISurface*				canvas;
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
	
	/**
	 * Parent window. Normally this is the actual parent in the window hierarchy, but sometimes
	 * it is instead the principle parent, e.g. for menus. Used for cascading events.
	 */
	struct GWMWindow_*			parent;
	
	/**
	 * Layout of the window (NULL indicating absolute positioning).
	 */
	GWMLayout*				layout;
	
	/**
	 * Called when the window is to be position by its parent layout manager. This function specifies
	 * the position and requested size of the window. The function may center the widget if it can't
	 * be expanded to the full requested size. If this function is NULL, centering happens by default.
	 * The layout manager of this widget is executed only if this function is not NULL though.
	 */
	void (*position)(struct GWMWindow_ *win, int x, int y, int width, int height);
	
	/**
	 * Get the minimum or preferred size of the window. If either of these functions is defined, both must
	 * be. If nethier are defined (NULL) then:
	 *  - If a layout manager for this window is specified, it is used to compute the sizes.
	 *  - Otherwise, the width and height of the window are used as both.
	 */
	void (*getMinSize)(struct GWMWindow_ *win, int *width, int *height);
	void (*getPrefSize)(struct GWMWindow_ *win, int *width, int *height);
	
	/**
	 * Current window flags set by libgwm.
	 */
	int					flags;
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
	int					symbol;
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
 * Flags for wtComps, indicating which template components to create.
 */
#define	GWM_WTC_MENUBAR				(1 << 0)

/**
 * Template descriptor for gwmCreateTemplate() and gwmDestroyTemplate().
 */
typedef struct
{
	/**
	 * Flags (GWM_WTC_*) indicating which components are to be created by gwmCreateTemplate() and later
	 * which must be destroyed by gwmDestroyTemplate(). You should initialize this before calling
	 * gwmCreateTemplate() and not touch it after that. This indicates which fields of this structure
	 * are actually present, for binary compatibility between versions!
	 */
	int					wtComps;
	
	/**
	 * The window to put the template in. This must be created by the application before calling gwmCreateTemplate();
	 * gwmDestoryTemplate() does NOT destroy it.
	 */
	GWMWindow*				wtWindow;
	
	/**
	 * Layout manager to use for the body of the window. This must be initialized before gwmCreateTemplate(),
	 * and gwmDestroyTemplate() does NOT destroy it. It must NOT be set as the layout of wtWindow;
	 * gwmCreateTemplate() will instead add it as a sub-layout of the template layout.
	 */
	GWMLayout*				wtBody;
	
	/**
	 * This field is set to the menubar if it is requested in wtComps.
	 */
	GWMWindow*				wtMenubar;
} GWMWindowTemplate;

/**
 * Initializes the GWM library. This must be called before using any other functions.
 * Returns 0 on success, or -1 on error.
 */
int gwmInit();

/**
 * Quit the GWM library, with proper cleanup.
 */
void gwmQuit();

/**
 * Post an event to the given window. Returns the final event status:
 *	GWM_EVSTATUS_OK if all handlers returned GWM_EVSTATUS_OK or GWM_EVSTATUS_CONT
 *	GWM_EVSTATUS_BREAK if some handler returned it
 */
int gwmPostEvent(GWMEvent *ev, GWMWindow *win);

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
 * Set the caption of a window.
 */
void gwmSetWindowCaption(GWMWindow *win, const char *caption);

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
 * Push a new event handler to the top of the handler stack for the given window.
 */
void gwmPushEventHandler(GWMWindow *win, GWMEventHandler handler, void *context);

/**
 * Starts the main loop. Returns after an event handler requests an exit by returning -1.
 * You must use this when using the widget toolkit.
 */
void gwmMainLoop();

/**
 * Creates a new button in the specified window, with absolute coordinates. Use is discouraged; use
 * gwmNewButton(), followed by appropriate calls to property-setting functions. Alternatively, you
 * could use a convenience wrapper such as gwmCreateStockButton() or gwmCreateButtonWithLabel().
 */
GWMWindow* gwmCreateButton(GWMWindow *parent, const char *text, int x, int y, int width, int flags);

/**
 * Create a new button in the specified window. This call should be followed by property-setting
 * calls.
 */
GWMWindow *gwmNewButton(GWMWindow *parent);

/**
 * Create a stock button with the given symbol.
 */
GWMWindow* gwmCreateStockButton(GWMWindow *parent, int symbol);

/**
 * Create a button with the given symbol and label. For common system buttons (stock buttons), use
 * gwmCreateStockButton() to get the correctly-translated label.
 */
GWMWindow* gwmCreateButtonWithLabel(GWMWindow *parent, int symbol, const char *label);

/**
 * Destroys a button.
 */
void gwmDestroyButton(GWMWindow *button);

/**
 * Set the label of a button.
 */
void gwmSetButtonLabel(GWMWindow *button, const char *label);

/**
 * Set button flags.
 */
void gwmSetButtonFlags(GWMWindow *button, int flags);

/**
 * Set the symbol of a button.
 */
void gwmSetButtonSymbol(GWMWindow *button, int symbol);

/**
 * Set the preferred width of a button. If set to 0 (the default), the preferred width is the same as
 * the minimum width required to fit the text.
 */
void gwmSetButtonPrefWidth(GWMWindow *button, int width);

/**
 * Set the callback function for a button. Return value is the same as for event handlers.
 */
typedef int (*GWMButtonCallback)(void *param);
void gwmSetButtonCallback(GWMWindow *button, GWMButtonCallback callback, void *param);

/**
 * Create a new message dialog. The 'parent' is primarily used for notifications, and may be NULL.
 */
GWMWindow* gwmNewMessageDialog(GWMWindow *parent);

/**
 * Set the text in a message dialog.
 */
void gwmSetMessageText(GWMWindow *msg, const char *text);

/**
 * Set the icon in a message dialog to be a specific viewport in a surface.
 */
void gwmSetMessageIconCustom(GWMWindow *msg, DDISurface *surf, int x, int y, int width, int height);

/**
 * Set the icon in a message dialog to a standard one (GWM_MBICON_*).
 */
void gwmSetMessageIconStd(GWMWindow *msg, int inum);

/**
 * Add a stock button to a message dialog. Returns 0 on success or GWM_ERR_NORC if the button limit has
 * been exhausted. The button limit is 4.
 */
int gwmMessageAddStockButton(GWMWindow *msg, int symbol);

/**
 * Add a button with a custom symbol and label to a message dialog. Returns 0 on success or GWM_ERR_NORC
 * if the button limit has been exhausted. The button limit is 4.
 */
int gwmMessageAddButton(GWMWindow *msg, int symbol, const char *label);

/**
 * Run a message dialog. This function destroys the message dialog, and returns the symbol of the button
 * which the user clicked.
 */
int gwmRunMessageDialog(GWMWindow *msg);

/**
 * Display a message box. Returns the symbol of the button which was clicked.
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
GWMWindow* gwmCreateTextField(GWMWindow *parent, const char *text, int x, int y, int width, int flags);

/**
 * Creates a new text field in the specified window.
 */
GWMWindow* gwmNewTextField(GWMWindow *parent);

/**
 * Destroys a text field.
 */
void gwmDestroyTextField(GWMWindow *txt);

/**
 * Gets the current number of bytes in a text field. You may use it to get the full text by
 * calling gwmReadTextField().
 *
 * DEPRECATED; how can this work with unicode?
 */
size_t gwmGetTextFieldSize(GWMWindow *field);

/**
 * Returns a READ-ONLY NUL-terminated UTF-8 string containing the text field text. This string must
 * not be used after further calls to text field functions are made, or if the main loop is used!
 * The text field continues to won this function.
 */
const char* gwmReadTextField(GWMWindow *field);

/**
 * Set the width of a text field.
 */
void gwmResizeTextField(GWMWindow *field, int width);

/**
 * Change the text in a text field. The expected encoding is UTF-8.
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
 * Set the flags of a text field.
 */
void gwmSetTextFieldFlags(GWMWindow *field, int flags);

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
 * large enough to store at least 128 window references. The id of the focused window will be zero
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
 * Creates a checkbox in the specified window. This call should be followed by property-setting functions.
 */
GWMWindow *gwmNewCheckbox(GWMWindow *parent);

/**
 * Set the label on a checkbox.
 */
void gwmSetCheckboxLabel(GWMWindow *checkbox, const char *text);

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
 * Set the symbol of a checkbox.
 */
void gwmSetCheckboxSymbol(GWMWindow *checkbox, int symbol);

/**
 * Create a new scroll bar.
 */
GWMWindow* gwmNewScrollbar(GWMWindow *parent);

/**
 * Destroy a scrollbar.
 */
void gwmDestroyScrollbar(GWMWindow *sbar);

/**
 * Change the flags of a scrollbar.
 */
void gwmSetScrollbarFlags(GWMWindow *sbar, int flags);

/**
 * Change the position of a scrollbar, clamped to the [0.0, 1.0] range.
 */
void gwmSetScrollbarPosition(GWMWindow *sbar, float pos);

/**
 * Set the length of a scrollbar, clamped to the [0.0, 1.0] range.
 */
void gwmSetScrollbarLength(GWMWindow *sbar, float len);

/**
 * Get the current position of a scrollbar, in the [0.0, 1.0] range.
 */
float gwmGetScrollbarPosition(GWMWindow *sbar);

/**
 * Change the size of a window. This frees, and hence invalidates, a previous return value from gwmGetWindowCanvas()!
 */
void gwmResizeWindow(GWMWindow *win, int width, int height);

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
 * Add a command to a menu, with the specified symbol. If 'label' is NULL, the symbol is used to choose a
 * stock label.
 */
void gwmMenuAddCommand(GWMMenu *menu, int symbol, const char *label, void *data);

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
GWMWindow* gwmCreateMenubar(GWMWindow *parent);

/**
 * Alias for gwmCreateMenubar().
 */
GWMWindow* gwmNewMenubar(GWMWindow *parent);

/**
 * Destroy a menubar.
 */
void gwmDestroyMenubar(GWMWindow *menubar);

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
 * Classify the character 'c' as either a whitespace (0) or other character (1). 'c' is a unicode codepoint.
 */
int gwmClassifyChar(long c);

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
 *
 * DEPRECATED
 */
GWMWindow* gwmCreateRadioButton(GWMWindow *parent, int x, int y, GWMRadioGroup *group, int value, int flags);

/**
 * Create a radio button within the specified group, with the specified parent.
 */
GWMWindow* gwmNewRadioButton(GWMWindow *parent, GWMRadioGroup *group);

/**
 * Change the value associated with a radio button; this is the value that the group will be set to when the
 * radio button is selected.
 */
void gwmSetRadioButtonValue(GWMWindow *radio, int value);

/**
 * Change the label of a radio button.
 */
void gwmSetRadioButtonLabel(GWMWindow *radio, const char *text);

/**
 * Set the symbol of a radio button.
 */
void gwmSetRadioButtonSymbol(GWMWindow *radio, int symbol);

/**
 * Destroy a radio button.
 */
void gwmDestroyRadioButton(GWMWindow *radio);

/**
 * Create a slider.
 */
GWMWindow* gwmNewSlider(GWMWindow *parent);

/**
 * Destroy a slider.
 */
void gwmDestroySlider(GWMWindow *slider);

/**
 * Set slider flags.
 */
void gwmSetSliderFlags(GWMWindow *slider, int flags);

/**
 * Set the value of a slider. This function clamps it to the [0.0, 1.0] range.
 */
void gwmSetSliderValue(GWMWindow *slider, float value);

/**
 * Get the value of a slider, in the [0.0, 1.0] range.
 */
float gwmGetSliderValue(GWMWindow *slider);

/**
 * Turn a window handle into a global window reference.
 */
void gwmGetGlobRef(GWMWindow *win, GWMGlobWinRef *ref);

/**
 * Take a screenshot of the specified window, and return a surface containing it. The surface will be in
 * screen pixel format. Returns NULL if the screenshot could not be taken.
 */
DDISurface* gwmScreenshotWindow(GWMGlobWinRef *ref);

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
 * Create a new option menu widget.
 */
GWMWindow* gwmNewOptionMenu(GWMWindow *parent);

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
 * Create a frame widget.
 */
GWMWindow* gwmNewFrame(GWMWindow *parent);

/**
 * Set the caption of a frame.
 */
void gwmSetFrameCaption(GWMWindow *frame, const char *parent);

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
 *	GWM_TYPE_INT - A int* pointer, directly into the shared area.
 *
 * On error, NULL is returned and 'errOut' is set to a GWM error code (GWM_ERR_*) if it's not NULL:
 *
 *	GWM_ERR_INVAL - Wrong type specified.
 *	GWM_ERR_NOENT - No such theme property.
 */
void* gwmGetThemeProp(const char *name, int type, int *errOut);

/**
 * Inform all applications and the window manager that the theme has changed.
 */
void gwmRetheme();

/**
 * Create an abstract layout manager. Typically only used internally by functions like gwmCreateBoxLayout()
 * etc. A layout created using this function is to be destroyed with gwmDestroyAbstractLayout().
 * Typically you want to use a type-specific creation (and destruction) functions like gwmCreateBoxLayout()
 * and gwmDestroyBoxLayout() instead.
 *
 * All fields are initially set to NULL. You *MUST* set all calculation functions before using the layout;
 * failure to do so results in undefined behaviour (typically NULL references).
 */
GWMLayout* gwmCreateAbstractLayout();

/**
 * Destroy an abstract layout. Note that you should always use the type-specific destruction function - for
 * example if you created the layout with gwmCreateBoxLayout(), it must be destroyed with gwmDestroyBoxLayout().
 */
void gwmDestroyAbstractLayout(GWMLayout *layout);

/**
 * Calculate the preffered size of the layout of the given window, resize the window to the preffered size, and
 * lay out all the children. This should be called when layouts have been set up and the window is soon to be
 * presented.
 */
void gwmFit(GWMWindow *win);

/**
 * Resize a window to a specific size, and lay out the children. If the size is less than the minimum, then the
 * minimum size is used instead.
 */
void gwmLayout(GWMWindow *win, int width, int height);

/**
 * Set the layout manager for a window.
 */
void gwmSetWindowLayout(GWMWindow *win, GWMLayout *layout);

/**
 * Create a box layout. The flags must be one of the following:
 *	GWM_BOX_HORIZONTAL - lay out children horizontally
 *	GWM_BOX_VERTICAL - lay out children vertically
 * You may also bitwise-OR zero or more of the following into it:
 *	GWM_BOX_INCLUDE_HIDDEN - allocate space for hidden children (not done by default).
 */
GWMLayout* gwmCreateBoxLayout(int flags);

/**
 * Destroy a box layout. It must not be currently assigned to any window (the window must be destroyed, or its
 * layout set to NULL or something else).
 */
void gwmDestroyBoxLayout(GWMLayout *layout);

/**
 * Add a window to a box layout.
 */
void gwmBoxLayoutAddWindow(GWMLayout *box, GWMWindow *win, int proportion, int border, int flags);

/**
 * Add a sub-layout to a box layout.
 */
void gwmBoxLayoutAddLayout(GWMLayout *box, GWMLayout *child, int proportion, int border, int flags);

/**
 * Given a stock symbol, return its label (in the correct language).
 * Returns "??" for invalid labels.
 */
const char *gwmGetStockLabel(int symbol);

/**
 * Get data associated with an event handler (may be NULL). Returns NULL if the event handler is not found.
 * This is used to store arbitrary class-specific data; the event handler is used as a class identifier.
 */
void* gwmGetData(GWMWindow *win, GWMEventHandler handler);

/**
 * Create a new label. This call should be followed by property-setting calls. Alternatively, you may use
 * the convenience wrapper, gwmCreateLabel().
 */
GWMWindow* gwmNewLabel(GWMWindow *parent);

/**
 * Destroy a label.
 */
void gwmDestroyLabel(GWMWindow *label);

/**
 * Convenience wrapper to create a label with properties.
 */
GWMWindow* gwmCreateLabel(GWMWindow *parent, const char *text, int width);

/**
 * Set the text of a label.
 */
void gwmSetLabelText(GWMWindow *label, const char *text);

/**
 * Set the maximum width of a label. 0 means there is no limit, and so it should not wrap.
 */
void gwmSetLabelWidth(GWMWindow *label, int width);

/**
 * Get the icon surface ID of a window.
 */
uint32_t gwmGetGlobIcon(GWMGlobWinRef *ref);

/**
 * Create a new image widget. This call should be followed by property-setting functions. Alternatively,
 * you may use the convenience wrapper, gwmCreateImage().
 */
GWMWindow* gwmNewImage(GWMWindow *parent);

/**
 * Destroy an image.
 */
void gwmDestroyImage(GWMWindow *image);

/**
 * Set the surface to be displayed on an image. NULL indicates no image.
 */
void gwmSetImageSurface(GWMWindow *image, DDISurface *surf);

/**
 * Set the viewport of an image. This indicates the offset into the surface, and size of the cropped area,
 * to be drawn. A width or height of 0 indicates the maximum extent along the given axis.
 */
void gwmSetImageViewport(GWMWindow *image, int x, int y, int width, int height);

/**
 * Convenience wrapper to create an image of the specified surface and viewport.
 */
GWMWindow* gwmCreateImage(GWMWindow *parent, DDISurface *surf, int x, int y, int width, int height);

/**
 * Create components based on the specified template. Return 0 on success, -1 on error.
 */
int gwmCreateTemplate(GWMWindowTemplate *wt);

/**
 * Destroy objects created by gwmCreateTemplate() for the specified template. Return 0 on success, -1 on error.
 */
int gwmDestroyTemplate(GWMWindowTemplate *wt);

/**
 * Generate a unique temporary symbol.
 */
int gwmGenerateSymbol();

/**
 * Create a new scale widget. This is an abstract class with no drawing or layouts; use its subclasses such as
 * GWMSlider or GWMScrollbar.
 */
GWMWindow* gwmNewScale(GWMWindow *parent);

/**
 * Destroy a scale.
 */
void gwmDestroyScale(GWMWindow *scale);

/**
 * Set the symbol of a scale.
 */
void gwmSetScaleSymbol(GWMWindow *scale, int symbol);

/**
 * Set the value of a scale. Clamped to the [0.0, 1.0] range.
 */
void gwmSetScaleValue(GWMWindow *scale, float value);

/**
 * Get the value of a scale, in the [0.0, 1.0] range.
 */
float gwmGetScaleValue(GWMWindow *scale);

#endif
