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
#include <pthread.h>

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
#define	GWM_WINDOW_NORESTACK			(1 << 7)

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
 * License.
 */
#define	GWM_LICENSE				"Glidix GUI\n\
\n\
Copyright (c) 2014-2017, Madd Games.\n\
All rights reserved.\n\
\n\
Redistribution and use in source and binary forms, with or without\n\
modification, are permitted provided that the following conditions are met:\n\
\n\
* Redistributions of source code must retain the above copyright notice, this\n\
  list of conditions and the following disclaimer.\n\
\n\
* Redistributions in binary form must reproduce the above copyright notice,\n\
  this list of conditions and the following disclaimer in the documentation\n\
  and/or other materials provided with the distribution.\n\
\n\
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS \"AS IS\"\n\
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n\
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE\n\
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE\n\
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL\n\
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR\n\
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER\n\
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,\n\
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n\
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"

/**
 * Colors.
 */
#define	GWM_COLOR_SELECTION			gwmColorSelectionP
extern DDIColor* gwmColorSelectionP;
#define	GWM_COLOR_BACKGROUND			gwmBackColorP
extern DDIColor* gwmBackColorP;
#define	GWM_COLOR_EDITOR			gwmEditorColorP
extern DDIColor* gwmEditorColorP;
#define	GWM_COLOR_BORDER_LIGHT			gwmBorderLightColorP
extern DDIColor* gwmBorderLightColorP;
#define	GWM_COLOR_BORDER_DARK			gwmBorderDarkColorP
extern DDIColor* gwmBorderDarkColorP;

/**
 * Fonts.
 */
#define	GWM_FONT_DEFAULT			gwmDefaultFontP
extern DDIFont* gwmDefaultFontP;
#define	GWM_FONT_CAPTION			gwmCaptionFontP
extern DDIFont* gwmCaptionFontP;
#define	GWM_FONT_STRONG				gwmStrongFontP
extern DDIFont* gwmStrongFontP;

/**
 * Unspecified window position.
 */
#define	GWM_POS_UNSPEC				((unsigned int)-1)

/**
 * Maximum timespan between 2 clicks that allows it to be considered a "double-click".
 */
#define	GWM_DOUBLECLICK_TIMEOUT			(CLOCKS_PER_SEC/2)

/**
 * Button flags.
 */
#define	GWM_BUTTON_DISABLED			(1 << 0)

/**
 * Text field flags.
 */
#define	GWM_TXT_DISABLED			(1 << 1)
#define	GWM_TXT_MASKED				(1 << 2)
#define	GWM_TXT_MULTILINE			(1 << 3)

/**
 * Label border styles.
 */
#define	GWM_BORDER_NONE				0
#define	GWM_BORDER_RAISED			1
#define	GWM_BORDER_SUNKEN			2

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
 * Data types for GWMDataCtrl.
 */
#define	GWM_DATA_STRING				1

/**
 * Values for 'whence' in gwmAddDataNode().
 */
#define	GWM_DATA_ADD_BEFORE			1
#define	GWM_DATA_ADD_AFTER			2
#define	GWM_DATA_ADD_TOP_CHILD			3
#define	GWM_DATA_ADD_BOTTOM_CHILD		4

/**
 * GWMDataCtrl flags.
 */
#define	GWM_DATA_SHOW_HEADERS			(1 << 0)

/**
 * GWMDataNode flags.
 */
#define	GWM_DATA_NODE_FORCE_PTR			(1 << 0)

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
 * Splitter flags. Again, GWM_SPLIITER_HORIZ and GWM_SPLITTER_VERT are mutually exclusive,
 * with GWM_SPLITTER_VERT being the default. The orientation refers to the axis on which the
 * splitter MOVES, not the axis on which it forms a line graphically (that is, a horizontal
 * splitter affects the width of its panels).
 */
#define	GWM_SPLITTER_VERT			0
#define	GWM_SPLITTER_HORIZ			(1 << 0)
#define	GWM_SPLITTER_DISABLED			(1 << 1)

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
#define	GWM_CURSOR_SPLIT_HORIZ			4
#define	GWM_CURSOR_SPLIT_VERT			5
#define	GWM_CURSOR_COUNT			6		/* number of cursors */

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
	
	/**
	 * Notebook tab image. Extends horizontally according to the "button" method (see imgButton).
	 * It is made of 3 vertically stacked 17x20 sprites, meaning (from top to bottom):
	 * Inactive, inactive-hovered, active.
	 */
	uint32_t				imgNotebook;
	
	/**
	 * Selection color.
	 */
	DDIColor				colSelection;
	
	/**
	 * Window default background color.
	 */
	DDIColor				colWinBack;
	
	/**
	 * Progress bar gradient.
	 */
	DDIColor				colProgressLeft;
	DDIColor				colProgressRight;
	
	/**
	 * Progress bar background.
	 */
	DDIColor				colProgressBackground;
	
	/**
	 * Tool button image. It is 3 vertically-stacked sprites, each 30x30. From top to
	 * bottom: normal, hovered, clicked.
	 */
	uint32_t				imgToolButton;
	
	/**
	 * Stock images.
	 */
	uint32_t				imgStockBack;
	uint32_t				imgStockForward;
	uint32_t				imgStockUp;
	uint32_t				imgStockDown;
	uint32_t				imgStockNewFile;
	uint32_t				imgStockOpenFile;
	uint32_t				imgStockSave;
	uint32_t				imgStockSaveAs;
	uint32_t				imgStockSaveCopy;
	uint32_t				imgStockRefresh;
	uint32_t				imgStockCancel;
	uint32_t				imgStockConfirm;
	uint32_t				imgStockCut;
	uint32_t				imgStockCopy;
	uint32_t				imgStockPaste;
	uint32_t				imgStockAdd;
	uint32_t				imgStockRemove;
	uint32_t				imgStockHome;
	uint32_t				imgStockUndo;
	uint32_t				imgStockRedo;
	uint32_t				imgStockAdjLeft;
	uint32_t				imgStockAdjCenter;
	uint32_t				imgStockAdjRight;
	uint32_t				imgStockJustify;
	uint32_t				imgStockSelectAll;
	uint32_t				imgStockEdit;
	uint32_t				imgStockBold;
	uint32_t				imgStockItalic;
	uint32_t				imgStockUnderline;
	uint32_t				imgStockStrike;
	uint32_t				imgStockResv[16];
	
	/**
	 * Editing area color.
	 */
	DDIColor				colEditor;
	
	/**
	 * Combo box image.
	 */
	uint32_t				imgCombo;
	
	/**
	 * Spinner controls image.
	 */
	uint32_t				imgSpin;
	
	/**
	 * Data control "tree pointer" image.
	 */
	uint32_t				imgTreePtr;
	
	/**
	 * Frame color.
	 */
	DDIColor				colFrame;
	
	/**
	 * "Light" and "dark" parts of a border.
	 */
	DDIColor				colBorderLight;
	DDIColor				colBorderDark;
	
	/**
	 * Syntax highlighting colors.
	 */
	DDIColor				colSyntaxKeyword;
	DDIColor				colSyntaxType;
	DDIColor				colSyntaxConstant;
	DDIColor				colSyntaxNumber;
	DDIColor				colSyntaxBuiltin;
	DDIColor				colSyntaxComment;
	DDIColor				colSyntaxString;
	DDIColor				colSyntaxPreproc;
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
 * Data node object for GWMDataCtrl. Opaque - use pointers only. Defined in datactrl.c.
 */
typedef struct GWMDataNode_ GWMDataNode;

/**
 * Data column object for GWMDataCtrl. Opaque - use pointers only. Defined in datactrl.c.
 */
typedef struct GWMDataColumn_ GWMDataColumn;

/**
 * Flags for the box layout.
 */
#define	GWM_BOX_HORIZONTAL			0		/* horizontal by default */
#define	GWM_BOX_VERTICAL			(1 << 0)	/* vertical with this flag */

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
 * Grid layout scaling.
 */
#define	GWM_GRID_CENTER				1		/* center child along specified axis */
#define	GWM_GRID_FILL				2		/* fill child on the specified axis */

/**
 * Flex layout scaling.
 */
#define	GWM_FLEX_CENTER				1		/* center child along specified axis */
#define	GWM_FLEX_FILL				2		/* fill child on the specified axis */

/**
 * Flags for gwmLayoutEx().
 */
#define	GWM_LAYOUT_CENTER			(1 << 0)

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
#define	GWM_SYM_BACK				6
#define	GWM_SYM_FORWARD				7
#define	GWM_SYM_UP				8
#define	GWM_SYM_DOWN				9
#define	GWM_SYM_NEW_FILE			10
#define	GWM_SYM_OPEN_FILE			11
#define	GWM_SYM_SAVE				12
#define	GWM_SYM_SAVE_AS				13
#define	GWM_SYM_SAVE_COPY			14
#define	GWM_SYM_REFRESH				15
#define	GWM_SYM_CONFIRM				16
#define	GWM_SYM_CUT				17
#define	GWM_SYM_COPY				18
#define	GWM_SYM_PASTE				19
#define	GWM_SYM_ADD				20
#define	GWM_SYM_REMOVE				21
#define	GWM_SYM_HOME				22
#define	GWM_SYM_UNDO				23
#define	GWM_SYM_REDO				24
#define	GWM_SYM_ADJ_LEFT			25
#define	GWM_SYM_ADJ_CENTER			26
#define	GWM_SYM_ADJ_RIGHT			27
#define	GWM_SYM_JUSTIFY				28
#define	GWM_SYM_SELECT_ALL			29
#define	GWM_SYM_EDIT				30
#define	GWM_SYM_BOLD				31
#define	GWM_SYM_ITALIC				32
#define	GWM_SYM_UNDERLINE			33
#define	GWM_SYM_STRIKE				34
#define	GWM_SYM_RENAME				35
#define	GWM_SYM_OPEN				36
#define	GWM_SYM_CLOSE				37
#define	GWM_SYM_ABOUT				38
#define	GWM_SYM_HELP				39
#define	GWM_SYM_HIGHSCORE			40

/**
 * Starting numbers for event classes.
 * When bit 24 is set, the event is cascading.
 * When bit 25 is set, the event is user-defined (otherwise libgwm built-in).
 * When bit 26 is set, the event aggreagtes (it is allowed to be ignored if more events of the same type arrive).
 */
#define	GWM_EVENT_CASCADING			(1 << 24)
#define	GWM_EVENT_USER				(1 << 25)
#define	GWM_EVENT_AGG				(1 << 26)

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
#define	GWM_EVENT_RESIZED			13
#define	GWM_EVENT_SPIN_DECR			14
#define	GWM_EVENT_SPIN_INCR			15
#define	GWM_EVENT_SPIN_SET			16

/**
 * Cascading events.
 */
#define	GWM_EVENT_COMMAND			(GWM_EVENT_CASCADING + 1)
#define	GWM_EVENT_TOGGLED			(GWM_EVENT_CASCADING + 2)
#define	GWM_EVENT_VALUE_CHANGED			(GWM_EVENT_CASCADING + 3)
#define	GWM_EVENT_TAB_LIST_UPDATED		(GWM_EVENT_CASCADING + 4)
#define	GWM_EVENT_TAB_FADING			(GWM_EVENT_CASCADING + 5)
#define	GWM_EVENT_TAB_FADED			(GWM_EVENT_CASCADING + 6)
#define	GWM_EVENT_TAB_ACTIVATED			(GWM_EVENT_CASCADING + 7)
#define	GWM_EVENT_EDIT_END			(GWM_EVENT_CASCADING + 8)
#define	GWM_EVENT_MENU_CLOSE			(GWM_EVENT_CASCADING + 9)
#define	GWM_EVENT_DATA_EXPANDING		(GWM_EVENT_CASCADING + 10)
#define	GWM_EVENT_DATA_COLLAPSING		(GWM_EVENT_CASCADING + 11)
#define	GWM_EVENT_DATA_ACTIVATED		(GWM_EVENT_CASCADING + 12)
#define	GWM_EVENT_DATA_DELETING			(GWM_EVENT_CASCADING + 13)
#define	GWM_EVENT_DATA_SELECT_CHANGED		(GWM_EVENT_CASCADING + 14)
#define	GWM_EVENT_OPTION_SET			(GWM_EVENT_CASCADING + 15)

/**
 * General event structure.
 * NOTE: This structure must not be edited, for binary compatibility reasons.
 * It may sometimes be cast to other types, such as GWMCommandEvent, depending on
 * the 'type' field.
 */
typedef struct
{
	int					type;
	int					value;		// arbitary value associated with event
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
 * Tab event structure. Aliased from GWMEvent when 'type' is GWM_EVENT_TAB_*.
 */
typedef struct
{
	GWMEvent				header;
	struct GWMWindow_*			tab;		// the tab in question if applicable, or NULL
} GWMTabEvent;

/**
 * Spin event structure. Emitted to implement actions for spinners.
 */
typedef struct
{
	GWMEvent				header;
	const char*				text;		// value string for GWM_EVENT_SPIN_SET
} GWMSpinEvent;

/**
 * Data control event.
 */
typedef struct
{
	GWMEvent				header;
	GWMDataNode*				node;		// node in question
	int					symbol;
} GWMDataEvent;

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
	
	/**
	 * Window caption.
	 */
	char*					caption;
	
	/**
	 * Previous and next windows in tab order.
	 */
	struct GWMWindow_*			tabPrev;
	struct GWMWindow_*			tabNext;
	
	/**
	 * The last child window in tab order.
	 */
	struct GWMWindow_*			tabLastChild;
	
	/**
	 * Does this window accept tabs?
	 */
	int					tabAccept;
} GWMObject;

/**
 * Typedef GWMObject to all classes.
 */
typedef GWMObject GWMWindow;
// TODO: typedef the rest, it's for clarity
typedef GWMWindow GWMButton;
typedef	GWMWindow GWMSplitter;
typedef GWMWindow GWMTextField;
typedef GWMWindow GWMToolButton;
typedef	GWMWindow GWMScrollbar;
typedef GWMWindow GWMCombo;
typedef GWMWindow GWMSpinner;
typedef GWMWindow GWMDataCtrl;
typedef	GWMWindow GWMLabel;
typedef GWMWindow GWMOptionMenu;
typedef GWMWindow GWMCheckbox;
typedef GWMWindow GWMModal;
typedef GWMWindow GWMImage;
typedef GWMWindow GWMSlider;
typedef GWMWindow GWMFrame;
typedef GWMWindow GWMTabList;
typedef GWMWindow GWMTab;

/**
 * Typedef tab lists.
 */
typedef GWMTabList GWMNotebook;

/**
 * Typedef modals.
 */
typedef GWMModal GWMFileChooser;
typedef GWMModal GWMAboutDialog;
typedef GWMModal GWMTextDialog;

/**
 * Typedef the spinner classes.
 */
typedef GWMSpinner GWMIntSpinner;

/**
 * GWM timer.
 */
typedef struct
{
	/**
	 * The thread handling the timing.
	 */
	pthread_t				thread;
	
	/**
	 * Timer period in milliseconds.
	 */
	int					period;
	
	/**
	 * Whether or not the timer should continue.
	 */
	int					running;
	
	/**
	 * The window to send update events to.
	 */
	GWMWindow*				win;
	
	/**
	 * Update event type to send (default GWM_EVENT_UPDATE).
	 */
	int					evtype;
} GWMTimer;

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
 * Flags for wtComps, indicating which template components to create.
 */
#define	GWM_WTC_MENUBAR				(1 << 0)
#define	GWM_WTC_TOOLBAR				(1 << 1)
#define	GWM_WTC_STATUSBAR			(1 << 2)

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
	
	/**
	 * This field is set to the toolbar layout manager if it is requested in wtComps.
	 */
	GWMLayout*				wtToolbar;
	
	/**
	 * These fields are set to the status bar and the "status label" if a GWM_WTC_STATUSBAR
	 * was requested.
	 */
	GWMLabel*				wtStatusLabel;
	GWMWindow*				wtStatusBar;
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
 * Throw an exception.
 */
void gwmThrow(int errcode);

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
 * Mark a window as accepting tabs.
 */
void gwmAcceptTabs(GWMWindow *win);

/**
 * Destroy a window.
 */
void gwmDestroyWindow(GWMWindow *win);

/**
 * Set the caption of a window.
 */
void gwmSetWindowCaption(GWMWindow *win, const char *caption);

/**
 * Return the window caption.
 */
const char* gwmGetWindowCaption(GWMWindow *win);

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
 * Queue up an event of the specified type, with the specified 'value'.
 */
void gwmPostUpdateEx(GWMWindow *win, int type, int value);

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
 * Give focus to the specified window.
 */
void gwmFocus(GWMWindow *win);

/**
 * Show a window.
 */
void gwmShow(GWMWindow *win);

/**
 * Hide a window.
 */
void gwmHide(GWMWindow *win);

/**
 * Creates a new text field in the specified window.
 */
GWMTextField* gwmCreateTextField(GWMWindow *parent, const char *text, int x, int y, int width, int flags);

/**
 * Creates a new text field in the specified window.
 */
GWMTextField* gwmNewTextField(GWMWindow *parent);

/**
 * Destroys a text field.
 */
void gwmDestroyTextField(GWMTextField *txt);

/**
 * Returns a READ-ONLY NUL-terminated UTF-8 string containing the text field text. This string must
 * not be used after further calls to text field functions are made, or if the main loop is used!
 * The text field continues to won this function.
 */
const char* gwmReadTextField(GWMTextField *field);

/**
 * Change the text in a text field. The expected encoding is UTF-8.
 */
void gwmWriteTextField(GWMTextField *field, const char *newText);

/**
 * Select the entire contents of the text field.
 */
void gwmTextFieldSelectAll(GWMTextField *field);

/**
 * Set the flags of a text field.
 */
void gwmSetTextFieldFlags(GWMTextField *field, int flags);

/**
 * Set the icon of a text field, to be displayed before the text. NULL means no icon. The icon
 * is taken as 16x16.
 */
void gwmSetTextFieldIcon(GWMTextField *field, DDISurface *icon);

/**
 * Set the placeholder text for the text field.
 */
void gwmSetTextFieldPlaceholder(GWMTextField *field, const char *placeholder);

/**
 * Set whether the given text field should wrap text. False by default.
 */
void gwmSetTextFieldWrap(GWMTextField *field, GWMbool wrap);

/**
 * Set text alignment in a text field (DDI_ALIGN_*). Left by default.
 */
void gwmSetTextFieldAlignment(GWMTextField *field, int align);

/**
 * Set the font on a text field.
 */
void gwmSetTextFieldFont(GWMTextField *field, DDIFont *font);

/**
 * Clear all styles currently applied in a text field.
 */
void gwmClearTextFieldStyles(GWMTextField *field);

/**
 * Set the foreground color in the specified range in a text field. The range is given in CHARACTER, not BYTE
 * units! (Each character may have a different length in UTF-8 encoding).
 */
void gwmSetTextFieldColorRange(GWMTextField *field, size_t start, size_t end, DDIColor *color);

/**
 * Insert text into a text field, at the cursor position.
 */
void gwmTextFieldInsert(GWMWindow *field, const char *str);

/**
 * Sets which cursor should be used by a window. The cursor is one of the GWM_CURSOR_* macros.
 * Returns 0 on success, -1 on error.
 */
int gwmSetWindowCursor(GWMWindow *win, int cursor);

/**
 * Sets the icon of a window. The specified surface must use the "GWM icon layout".
 */
int gwmSetWindowIcon(GWMWindow *win, DDISurface *icon);

/**
 * Get the current icon surface of the specified window.
 */
DDISurface* gwmGetWindowIcon(GWMWindow *win);

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
GWMCheckbox *gwmCreateCheckbox(GWMWindow *parent, int x, int y, int state, int flags);

/**
 * Creates a checkbox in the specified window. This call should be followed by property-setting functions.
 */
GWMCheckbox *gwmNewCheckbox(GWMWindow *parent);

/**
 * Set the label on a checkbox.
 */
void gwmSetCheckboxLabel(GWMCheckbox *checkbox, const char *text);

/**
 * Destroys a checkbox.
 */
void gwmDestroyCheckbox(GWMCheckbox *checkbox);

/**
 * Set checkbox flags.
 */
void gwmSetCheckboxFlags(GWMCheckbox *checkbox, int flags);

/**
 * Return the current state of a checkbox (GWM_CB_OFF, GWM_CB_ON, or GWM_CB_TRI).
 */
int gwmGetCheckboxState(GWMCheckbox *checkbox);

/**
 * Set the state of a checkbox (GWM_CB_OFF, GWM_CB_ON or GWM_CB_TRI).
 */
void gwmSetCheckboxState(GWMCheckbox *checkbox, int state);

/**
 * Set the symbol of a checkbox.
 */
void gwmSetCheckboxSymbol(GWMCheckbox *checkbox, int symbol);

/**
 * Create a new scroll bar.
 */
GWMScrollbar* gwmNewScrollbar(GWMWindow *parent);

/**
 * Destroy a scrollbar.
 */
void gwmDestroyScrollbar(GWMScrollbar *sbar);

/**
 * Change the flags of a scrollbar.
 */
void gwmSetScrollbarFlags(GWMScrollbar *sbar, int flags);

/**
 * Change the position of a scrollbar, clamped to the [0.0, 1.0] range.
 */
void gwmSetScrollbarPosition(GWMScrollbar *sbar, float pos);

/**
 * Set the length of a scrollbar, clamped to the [0.0, 1.0] range.
 */
void gwmSetScrollbarLength(GWMScrollbar *sbar, float len);

/**
 * Get the current position of a scrollbar, in the [0.0, 1.0] range.
 */
float gwmGetScrollbarPosition(GWMScrollbar *sbar);

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
 * Create a new tab list. This is an abstract class; use its derivaties such as GWMNotebook.
 */
GWMWindow* gwmNewTabList(GWMWindow *parent);

/**
 * Destroy a tab list.
 */
void gwmDestroyTabList(GWMWindow *tablist);

/**
 * Create a new tab on a tab list.
 */
GWMWindow* gwmNewTab(GWMWindow *tablist);

/**
 * Destroy a tab from a tab list.
 */
void gwmDestroyTab(GWMWindow *tab);

/**
 * Activate the specified tab (switch to it). This goes as follows:
 * 1. A GWM_EVENT_TAB_FADING is emitted for the current tab, and if inhibited, no switch occurs.
 * 2. If the event reaches the GWMTabList handler, then a GWM_EVENT_TAB_FADED is emitted for the orignal tab;
 * 3. then, a GWM_EVENT_TAB_LIST_UPDATED is emitted, so that the subclass could update the visible tab and
 *    decoration as necessary.
 * 4. A GWM_EVENT_TAB_ACTIVATED is emitted for the new tab.
 *
 * NOTE: All events are emitted on the TAB LIST, not the tab itself! The 'tab' field of the GWMTabEvent
 *       is set to the tab in question though.
 */
void gwmActivateTab(GWMWindow *tab);

/**
 * Return the active tab of the tablist.
 */
GWMWindow* gwmGetActiveTab(GWMWindow *tablist);

/**
 * Get a tab given an index. Used to enumerate tabs. Returns NULL if out of bounds.
 */
GWMWindow* gwmGetTab(GWMWindow *tablist, int index);

/**
 * Create a new notebook widget with the specified parent.
 */
GWMWindow* gwmNewNotebook(GWMWindow *parent);

/**
 * Destroy a notebook.
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
GWMSlider* gwmNewSlider(GWMWindow *parent);

/**
 * Destroy a slider.
 */
void gwmDestroySlider(GWMSlider *slider);

/**
 * Set slider flags.
 */
void gwmSetSliderFlags(GWMSlider *slider, int flags);

/**
 * Set the value of a slider. This function clamps it to the [0.0, 1.0] range.
 */
void gwmSetSliderValue(GWMSlider *slider, float value);

/**
 * Get the value of a slider, in the [0.0, 1.0] range.
 */
float gwmGetSliderValue(GWMSlider *slider);

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
 * Create a combo box.
 */
GWMCombo* gwmNewCombo(GWMWindow *parent);

/**
 * Destroy a combo box.
 */
void gwmDestroyCombo(GWMCombo *combo);

/**
 * Add an option to a combo box.
 */
void gwmAddComboOption(GWMCombo *combo, const char *option);

/**
 * Clear the options of a combo box.
 */
void gwmClearComboOptions(GWMCombo *combo);

/**
 * Change the text of a combo box. The text is UTF-8 encoded.
 */
void gwmWriteCombo(GWMCombo *combo, const char *text);

/**
 * Get the text of a combo box.
 */
const char* gwmReadCombo(GWMCombo *combo);

/**
 * Create an option menu widget.
 */
GWMOptionMenu* gwmCreateOptionMenu(GWMWindow *parent, uint64_t id, const char *text, int x, int y, int width, int flags);

/**
 * Create a new option menu widget.
 */
GWMOptionMenu* gwmNewOptionMenu(GWMWindow *parent);

/**
 * Destroy an option menu.
 */
void gwmDestroyOptionMenu(GWMOptionMenu *optmenu);

/**
 * Clear an option menu.
 */
void gwmClearOptionMenu(GWMOptionMenu *optmenu);

/**
 * Change the value in an option menu.
 */
void gwmSetOptionMenu(GWMOptionMenu *optmenu, uint64_t id, const char *text);

/**
 * Add an option to an option menu.
 */
void gwmAddOptionMenu(GWMOptionMenu *optmenu, uint64_t id, const char *text);

/**
 * Get the ID of the currently-selected option in the menu.
 */
uint64_t gwmReadOptionMenu(GWMOptionMenu *optmenu);

/**
 * Create a frame widget with the specified caption.
 */
GWMWindow* gwmCreateFrame(GWMWindow *parent, const char *caption, int x, int y, int width, int height);

/**
 * Create a frame widget.
 */
GWMFrame* gwmNewFrame(GWMWindow *parent);

/**
 * Set the caption of a frame.
 */
void gwmSetFrameCaption(GWMFrame *frame, const char *caption);

/**
 * Destroy a frame widget.
 */
void gwmDestroyFrame(GWMFrame *frame);

/**
 * Get the panel of the specified frame, into which children may be placed to put them in the frame.
 * The panel is owned by the frame; do not delete it. It will be deleted when the frame is deleted.
 */
GWMWindow* gwmGetFramePanel(GWMFrame *frame);

/**
 * Create a file chooser dialog. 'mode' is either GWM_FILE_OPEN or GWM_FILE_SAVE. You can set other settings then call
 * gwmRunFileChooser() to run it.
 */
GWMFileChooser* gwmCreateFileChooser(GWMWindow *parent, const char *caption, int mode);

/**
 * Set the suggested file name on a file chooser.
 */
void gwmSetFileChooserName(GWMFileChooser *fc, const char *filename);

/**
 * Add a file type filter to a file chooser.
 */
void gwmAddFileChooserFilter(GWMFileChooser *fc, const char *label, const char *filtspec, const char *ext);

/**
 * Run a file chooser dialog. If the user clicks Cancel, NULL is returned; otherwise, the absolute path of the selected
 * file is returned, as a string on the heap; you must call free() on it later.
 *
 * NOTE: The file chooser is destroyed after running!
 */
char* gwmRunFileChooser(GWMFileChooser *fc);

/**
 * Get a file icon with the given name, in the given size (GWM_FICON_SMALL being 16x16 and GWM_FICON_LARGE being 64x64).
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
 * minimum size is used instead. If the window is top-level, it is also centered.
 */
void gwmLayout(GWMWindow *win, int width, int height);

/**
 * Resize a window to a specific size and lay out the children. Acceptable flags:
 * 	GWM_LAYOUT_CENTER	Center the window on the screen, if it has no parent.
 */
void gwmLayoutEx(GWMWindow *win, int width, int height, int flags);

/**
 * Set the layout manager for a window.
 */
void gwmSetWindowLayout(GWMWindow *win, GWMLayout *layout);

/**
 * Create a box layout. The flags must be one of the following:
 *	GWM_BOX_HORIZONTAL - lay out children horizontally
 *	GWM_BOX_VERTICAL - lay out children vertically
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
 * Add a spacer to a box layout.
 */
void gwmBoxLayoutAddSpacer(GWMLayout *box, int proportion, int border, int flags);

/**
 * Create a leaf layout. If 'win' is NULL, then this layout represents a spacer. Otherwise, it represents the
 * specified window. This is simply used to represents the leafs in a layout tree, i.e. the windows and the
 * spacers.
 */
GWMLayout* gwmCreateLeafLayout(GWMWindow *win);

/**
 * Destroy a leaf layout.
 */
void gwmDestroyLeafLayout(GWMLayout *leaf);

/**
 * Create a static layout.
 */
GWMLayout* gwmCreateStaticLayout(int minWidth, int minHeight, int prefWidth, int prefHeight);

/**
 * Destroy a static layout.
 */
void gwmDestroyStaticLayout(GWMLayout *layout);

/**
 * Add a nested layout to a static layout.
 */
void gwmStaticLayoutAddLayout(GWMLayout *st, GWMLayout *child, int left, int top, int right, int bottom);

/**
 * Add a window to a static layout.
 */
void gwmStaticLayoutAddWindow(GWMLayout *st, GWMWindow *child, int left, int top, int right, int bottom);

/**
 * Create a grid layout with the specified number of columns.
 */
GWMLayout* gwmCreateGridLayout(int cols);

/**
 * Add a sub-layout to a grid layout.
 */
void gwmGridLayoutAddLayout(GWMLayout *grid, GWMLayout *child, int colspan, int rowspan, int xScaling, int yScaling);

/**
 * Add a window to a grid layout.
 */
void gwmGridLayoutAddWindow(GWMLayout *grid, GWMWindow *child, int colspan, int rowspan, int xScaling, int yScaling);

/**
 * Destroy a grid layout.
 */
void gwmDestroyGridLayout(GWMLayout *grid);

/**
 * Create a flex layout with the specified number of columns.
 */
GWMLayout* gwmCreateFlexLayout(int cols);

/**
 * Add a sub-layout to a flex layout.
 */
void gwmFlexLayoutAddLayout(GWMLayout *flex, GWMLayout *sublayout, int xProp, int yProp, int xScaling, int yScaling);

/**
 * Add a window to a flex layout.
 */
void gwmFlexLayoutAddWindow(GWMLayout *flex, GWMWindow *child, int xProp, int yProp, int xScaling, int yScaling);

/**
 * Destroy a flex layout.
 */
void gwmDestroyFlexLayout(GWMLayout *flex);

/**
 * Given a stock symbol, return its label (in the correct language).
 * Returns "??" for invalid symbols.
 */
const char *gwmGetStockLabel(int symbol);

/**
 * Get a (24x24) stock icon for the given symbol. Returns NULL if no icon is available.
 */
DDISurface* gwmGetStockIcon(int symbol);

/**
 * Get data associated with an event handler (may be NULL). Returns NULL if the event handler is not found.
 * This is used to store arbitrary class-specific data; the event handler is used as a class identifier.
 */
void* gwmGetData(GWMWindow *win, GWMEventHandler handler);

/**
 * Create a new label. This call should be followed by property-setting calls. Alternatively, you may use
 * the convenience wrapper, gwmCreateLabel().
 */
GWMLabel* gwmNewLabel(GWMWindow *parent);

/**
 * Destroy a label.
 */
void gwmDestroyLabel(GWMLabel *label);

/**
 * Convenience wrapper to create a label with properties.
 */
GWMLabel* gwmCreateLabel(GWMWindow *parent, const char *text, int width);

/**
 * Set the text of a label.
 */
void gwmSetLabelText(GWMLabel *label, const char *text);

/**
 * Set the maximum width of a label. 0 means there is no limit, and so it should not wrap.
 */
void gwmSetLabelWidth(GWMLabel *label, int width);

/**
 * Set the font of a label.
 */
void gwmSetLabelFont(GWMLabel *label, DDIFont *font);

/**
 * Set the alignment of a label.
 */
void gwmSetLabelAlignment(GWMLabel *label, int align);

/**
 * Set the border of a label.
 */
void gwmSetLabelBorder(GWMLabel *label, int style, int width);

/**
 * Get the icon surface ID of a window.
 */
uint32_t gwmGetGlobIcon(GWMGlobWinRef *ref);

/**
 * Create a new image widget. This call should be followed by property-setting functions. Alternatively,
 * you may use the convenience wrapper, gwmCreateImage().
 */
GWMImage* gwmNewImage(GWMWindow *parent);

/**
 * Destroy an image.
 */
void gwmDestroyImage(GWMImage *image);

/**
 * Set the surface to be displayed on an image. NULL indicates no image.
 */
void gwmSetImageSurface(GWMImage *image, DDISurface *surf);

/**
 * Set the viewport of an image. This indicates the offset into the surface, and size of the cropped area,
 * to be drawn. A width or height of 0 indicates the maximum extent along the given axis.
 */
void gwmSetImageViewport(GWMImage *image, int x, int y, int width, int height);

/**
 * Convenience wrapper to create an image of the specified surface and viewport.
 */
GWMImage* gwmCreateImage(GWMWindow *parent, DDISurface *surf, int x, int y, int width, int height);

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

/**
 * Create a new progress bar.
 */
GWMWindow* gwmNewProgressBar(GWMWindow *parent);

/**
 * Destroy a progress bar.
 */
void gwmDestroyProgressBar(GWMWindow *progbar);

/**
 * Create a new tool button.
 */
GWMWindow* gwmNewToolButton(GWMWindow *parent);

/**
 * Destroy a tool button.
 */
void gwmDestroyToolButton(GWMWindow *toolbtn);

/**
 * Change the symbol of a tool button.
 */
void gwmSetToolButtonSymbol(GWMWindow *toolbtn, int symbol);

/**
 * Change the icon of a tool button. It must be 24x24. Set to NULL to use the default icon based on the symbol.
 */
void gwmSetToolButtonIcon(GWMWindow *toolbtn, DDISurface *icon);

/**
 * Add a tool button with a specific symbol to the specified toolbar.
 */
GWMToolButton* gwmAddToolButtonBySymbol(GWMWindow *parent, GWMLayout *toolbar, int symbol);

/**
 * Create a new splitter widget.
 */
GWMSplitter* gwmNewSplitter(GWMWindow *parent);

/**
 * Destroy a splitter widget.
 */
void gwmDestroySplitter(GWMSplitter *split);

/**
 * Set the flags of a splitter.
 */
void gwmSetSplitterFlags(GWMSplitter *split, int flags);

/**
 * Get a panel from a splitter. The index must be 0 or 1. For horizontal splitters, 0 is the left panel, and 1
 * is the right panel; for a vertical splitter, 0 is top and 1 is bottom.
 */
GWMWindow* gwmGetSplitterPanel(GWMSplitter *split, int index);

/**
 * Set the proportion of a splitter. Clamped to the [0.0,1.0] range.
 */
void gwmSetSplitterProportion(GWMSplitter *split, float prop);

/**
 * Get the current proportion of a splitter.
 */
float gwmGetSplitterProportion(GWMSplitter *split);

/**
 * Create a timer. This will periodically send the GWM_EVENT_UPDATE event to the specified window, until
 * destroyed with gwmDestroyTimer().
 */
GWMTimer* gwmCreateTimer(GWMWindow *win, int period);

/**
 * Same as gwmCreateTimer() except it sends a specific event.
 */
GWMTimer* gwmCreateTimerEx(GWMWindow *win, int period, int evtype);

/**
 * Destroy a timer. NOTE: spurious GWM_EVENT_UPDATE events may be sent to the window before the timer is
 * destroyed.
 */
void gwmDestroyTimer(GWMTimer *timer);

/**
 * Create a new abstract spinner. Do not use directly: instead use a derived class such as GWMIntSpinner.
 */
GWMSpinner* gwmNewSpinner(GWMWindow *parent);

/**
 * Destroy an abstract spinner.
 */
void gwmDestroySpinner(GWMSpinner *spin);

/**
 * Update the label on the spinner.
 */
void gwmSetSpinnerLabel(GWMSpinner *spin, const char *label);

/**
 * Create an integer spinner.
 */
GWMIntSpinner* gwmNewIntSpinner(GWMWindow *parent);

/**
 * Destroy an integer spinner.
 */
void gwmDestroyIntSpinner(GWMIntSpinner *ispin);

/**
 * Set the value of an integer spinner.
 */
void gwmSetIntSpinnerValue(GWMIntSpinner *ispin, int value);

/**
 * Change the format of an integer spinner.
 */
void gwmSetIntSpinnerFormat(GWMIntSpinner *ispin, const char *format);

/**
 * Get the value of an integer spinner.
 */
int gwmGetIntSpinnerValue(GWMIntSpinner *ispin);

/**
 * Set the value corresponding to a string key in a data node. This function makes an internal copy of the
 * string.
 */
void gwmSetDataString(GWMDataCtrl *ctrl, GWMDataNode *node, int key, const char *str);

/**
 * Return the value corresponding to a string key in a data node. Valid until updated. Returns an empty string
 * if the key has not been set.
 */
const char* gwmGetDataString(GWMDataCtrl *ctrl, GWMDataNode *node, int key);

/**
 * Set the icon of a data node. The icon must be a 16x16 surface (or GWM icon format).
 */
void gwmSetDataNodeIcon(GWMDataCtrl *ctrl, GWMDataNode *node, DDISurface *icon);

/**
 * Create a new data control widget with the specified parent.
 */
GWMDataCtrl* gwmNewDataCtrl(GWMWindow *parent);

/**
 * Set the flags in a data control.
 */
void gwmSetDataFlags(GWMDataCtrl *ctrl, int flags);

/**
 * Set the symbol of a data control.
 */
void gwmSetDataSymbol(GWMDataCtrl *ctrl, int symbol);

/**
 * Create a new column on the data control. It will have the specified label and data type, and will pull its values
 * from the specified key on a data node.
 */
GWMDataColumn* gwmAddDataColumn(GWMDataCtrl *ctrl, const char *label, int type, int key);

/**
 * Set the width of a column.
 */
void gwmSetDataColumnWidth(GWMDataCtrl *ctrl, GWMDataColumn* col, int width);

/**
 * Create a new data node on a data control. 'ctrl' is the data control in question. 'ref' is a reference node, or
 * NULL for the root node; then 'whence' specifies where the node shall be in relation to 'ref':
 *	GWM_DATA_ADD_BEFORE		The new node will have the same parent as 'ref' and come directly before it.
 *	GWM_DATA_ADD_AFTER		The new node will have the same parent as 'ref' and come directly after it.
 *	GWM_DATA_ADD_TOP_CHILD		The new node will be the first child of 'ref', with all remaining children coming after it.
 *	GWM_DATA_ADD_BOTTOM_CHILD	The new node will be the last child of 'ref', with all remaining children coming before it.
 * NOTE: If 'ref' is NULL, then 'whence' must be one of the *_CHILD macros, since there can be no siblings of the root node.
 */
GWMDataNode* gwmAddDataNode(GWMDataCtrl *ctrl, int whence, GWMDataNode *ref);

/**
 * Get a child of the specified node with the given index. A 'parent' of NULL means the root node. Returns a node if found,
 * or NULL if no such node exists.
 */
GWMDataNode* gwmGetDataNode(GWMDataCtrl *ctrl, GWMDataNode *parent, int index);

/**
 * Set flags on a data node.
 */
void gwmSetDataNodeFlags(GWMDataCtrl *ctrl, GWMDataNode *node, int flags);

/**
 * Set the description of a data node. The meaning is user-defined.
 */
void gwmSetDataNodeDesc(GWMDataCtrl *ctrl, GWMDataNode *node, void *desc);

/**
 * Get the description of a data node. The meaning is user-defined; it must have been previously set by gwmSetDataNodeDesc().
 */
void* gwmGetDataNodeDesc(GWMDataCtrl *ctrl, GWMDataNode *node);

/**
 * Delete a data node.
 */
void gwmDeleteDataNode(GWMDataCtrl *ctrl, GWMDataNode *node);

/**
 * Destroy a data control.
 */
void gwmDestroyDataCtrl(GWMDataCtrl *ctrl);

/**
 * Get selection on a data control. 'index' is how many selections to skip. Thus, if multiple nodes are selected, you can
 * enumerate all of them by iterating through 'index' from 0 up to the first one that returns no result. Returns a selected
 * node if found, or NULL if not found (at that index).
 */
GWMDataNode* gwmGetDataSelection(GWMDataCtrl *ctrl, int index);

/**
 * Create a new text dialog.
 */
GWMTextDialog* gwmNewTextDialog(GWMWindow *parent);

/**
 * Set the caption of a text dialog.
 */
void gwmSetTextDialogCaption(GWMTextDialog *txt, const char *caption);

/**
 * Set the text in a text dialog.
 */
void gwmSetTextDialog(GWMTextDialog *txt, const char *text);

/**
 * Run a text dialog.
 * NOTE: This also destroys it.
 */
void gwmRunTextDialog(GWMTextDialog *txt);

/**
 * A quick convenience wrapper to display text dialogs.
 */
void gwmTextDialog(GWMWindow *parent, const char *caption, const char *text);

/**
 * Create a new "about" dialog.
 */
GWMAboutDialog* gwmNewAboutDialog(GWMWindow *parent);

/**
 * Set the icon of an about dialog.
 */
void gwmSetAboutIcon(GWMAboutDialog *about, DDISurface *icon, int x, int y, int width, int height);

/**
 * Set the caption of an about dialog.
 */
void gwmSetAboutCaption(GWMAboutDialog *about, const char *caption);

/**
 * Set the description in an "about" dialog.
 */
void gwmSetAboutDesc(GWMAboutDialog *about, const char *desc);

/**
 * Set credits in an "about" dialog.
 */
void gwmSetAboutCredits(GWMAboutDialog *about, const char *credits);

/**
 * Set the license in an "about" dialog.
 */
void gwmSetAboutLicense(GWMAboutDialog *about, const char *license);

/**
 * Run an about dialog. Returns once the user closes the dialog.
 * NOTE: This also destroys the dialog.
 */
void gwmRunAbout(GWMAboutDialog *about);

/**
 * Display a color picker dialog. 'color' points to the current color (which will be displayed as the "old" color,
 * as well as the default.
 *
 * If the user clicks OK, this function returns GWM_SYM_OK, and sets 'color' to the color chosen by the user.
 * If the user clicks Cancel, this function returns GWM_SYM_CANCEL, and doe snot update the color.
 */
int gwmPickColor(GWMWindow *parent, const char *caption, DDIColor *color);

/**
 * Create a label in a status bar.
 */
GWMLabel* gwmNewStatusBarLabel(GWMWindow *statbar);

/**
 * Add the requested window to the status bar.
 */
void gwmAddStatusBarWindow(GWMWindow *statbar, GWMWindow *child);

/**
 * Display a "high score" dialog. If 'score' is -1, then the scores are simply displayed; if it is a positive integer,
 * and the score would actually get on the table, the user is asked for their name and added to it. 'gameName' is simply
 * the name of a game, and is used to decide which file should store the score.
 */
void gwmHighScore(const char *gameName, int score);

#endif
