/*
	Glidix kernel

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

#ifndef __glidix_humin_h
#define __glidix_humin_h

/**
 * This file contains data structures and routines which form a uniform interface for all
 * human input devices, such as keyboards, mice, joysticks, touch screens, etc. When a driver
 * detects and registers an input device, a /dev/huminX file (with X being a number) is
 * created, which may be opened ("acquired") by one process at a time. When such a device is
 * not opened, it is used by the kernel for terminal input. Otherwise, the process that opened
 * the file - called the "controlling process", can call read() on the file descriptor to
 * wait for input events, as described below. When the file is closed, it is released back
 * to the system.
 */

#include <glidix/common.h>
#include <glidix/devfs.h>
#include <glidix/semaphore.h>

/**
 * Event types.
 */
#define	HUMIN_EV_BUTTON_UP			1
#define	HUMIN_EV_BUTTON_DOWN			2

/**
 * Standard scancodes. Keyboards output scancodes in the range 0-255.
 * Mouse uses 0x100-0x1FF.
 * Special ones defined here.
 * Scancodes are hardware-specific; TODO: remove the scancode macros
 * completly and work with only keycodes.
 */
#define	HUMIN_SC_MOUSE_LEFT			0x100
#define	HUMIN_SC_MOUSE_MIDDLE			0x101
#define	HUMIN_SC_MOUSE_RIGHT			0x102

/**
 * Virtual keycodes. Those are hardware-independent, and the keyboard driver is responsible
 * for translating scancodes into virtual keycodes. The ASCII-mapped keycodes shall have values
 * corresponding to the character entered when the key is pressed without any modifiers, and with
 * all toggles (caps lock, etc) turned off, on a US keyboard. This does not include the numpad keys.
 * The range for virtual keycodes is 0x000-0x1FF inclusive.
 */
// START WITH ASCII-MAPPED KEYCODES
#define	HUMIN_KC_NULL				0x000		/* "no key" or unmapped scancode */
#define	HUMIN_KC_ESC				0x01B		/* escape (\e) */
#define	HUMIN_KC_BACKTICK			0x060		/* backtick (`) key */
#define	HUMIN_KC_1				0x031		/* number keys: */
#define	HUMIN_KC_2				0x032
#define	HUMIN_KC_3				0x033
#define	HUMIN_KC_4				0x034
#define	HUMIN_KC_5				0x035
#define	HUMIN_KC_6				0x036
#define	HUMIN_KC_7				0x037
#define	HUMIN_KC_8				0x038
#define	HUMIN_KC_9				0x039
#define	HUMIN_KC_0				0x030
#define	HUMIN_KC_HYPHEN				0x02D		/* hyphen/underspace key */
#define	HUMIN_KC_EQUALS				0x03D		/* equals/plus key */
#define	HUMIN_KC_BACKSPACE			0x008		/* backspace (\b) */
#define	HUMIN_KC_TAB				0x009		/* tab (\t) */
#define	HUMIN_KC_Q				0x071
#define	HUMIN_KC_W				0x077
#define	HUMIN_KC_E				0x065
#define	HUMIN_KC_R				0x072
#define	HUMIN_KC_T				0x074
#define	HUMIN_KC_Y				0x079
#define	HUMIN_KC_U				0x075
#define	HUMIN_KC_I				0x069
#define	HUMIN_KC_O				0x06F
#define	HUMIN_KC_P				0x070
#define	HUMIN_KC_SQB_LEFT			0x05B		/* [{ key */
#define	HUMIN_KC_SQB_RIGHT			0x05D		/* ]} key */
#define	HUMIN_KC_RETURN				0x00D		/* carriage return (\r) */
#define	HUMIN_KC_ENTER				HUMIN_KC_RETURN	/* alias */
#define	HUMIN_KC_A				0x061
#define	HUMIN_KC_S				0x073
#define	HUMIN_KC_D				0x064
#define	HUMIN_KC_F				0x066
#define	HUMIN_KC_G				0x067
#define	HUMIN_KC_H				0x068
#define	HUMIN_KC_J				0x06A
#define	HUMIN_KC_K				0x06B
#define	HUMIN_KC_L				0x06C
#define	HUMIN_KC_SEMICOLON			0x03B		/* ;: key */
#define	HUMIN_KC_QUOTE				0x027		/* '" key */
#define	HUMIN_KC_BACKSLASH			0x05C		/* \| key */
#define	HUMIN_KC_ARROWS				0x03C		/* |\ or <> key (left of Z or left of RETURN) */
#define	HUMIN_KC_Z				0x07A
#define	HUMIN_KC_X				0x078
#define	HUMIN_KC_C				0x063
#define	HUMIN_KC_V				0x076
#define	HUMIN_KC_B				0x062
#define	HUMIN_KC_N				0x06E
#define	HUMIN_KC_M				0x06D
#define	HUMIN_KC_COMMA				0x02C		/* ,< key */
#define	HUMIN_KC_PERIOD				0x02E		/* .> key */
#define	HUMIN_KC_SLASH				0x02F		/* /? key */
#define	HUMIN_KC_SPACE				0x020		/* space bar */
// NOW ONTO NON-ASCII KEYCODES
#define	HUMIN_KC_LEFT				0x100		/* left arrow key */
#define	HUMIN_KC_RIGHT				0x101		/* right arrow key */
#define	HUMIN_KC_UP				0x102		/* up arrow key */
#define	HUMIN_KC_DOWN				0x103		/* down arrow key */
#define	HUMIN_KC_LCTRL				0x104		/* left ctrl */
#define	HUMIN_KC_LSHIFT				0x105		/* left shift */
#define	HUMIN_KC_LALT				0x106		/* left alt */
#define	HUMIN_KC_RCTRL				0x107		/* right ctrl */
#define	HUMIN_KC_RSHIFT				0x108		/* right shift */
#define	HUMIN_KC_RALT				0x109		/* right alt, or alt gr */
#define	HUMIN_KC_F1				0x10A
#define	HUMIN_KC_F2				0x10B
#define	HUMIN_KC_F3				0x10C
#define	HUMIN_KC_F4				0x10D
#define	HUMIN_KC_F5				0x10E
#define	HUMIN_KC_F6				0x10F
#define	HUMIN_KC_F7				0x110
#define	HUMIN_KC_F8				0x111
#define	HUMIN_KC_F9				0x112
#define	HUMIN_KC_F10				0x113
#define	HUMIN_KC_F11				0x114
#define	HUMIN_KC_F12				0x115
#define	HUMIN_KC_PRINT_SCREEN			0x116		/* print screen / sysrq */
#define	HUMIN_KC_SCROLL_LOCK			0x117
#define	HUMIN_KC_PAUSE				0x118
#define	HUMIN_KC_INSERT				0x119
#define	HUMIN_KC_HOME				0x11A
#define	HUMIN_KC_PAGE_UP			0x11B
#define	HUMIN_KC_NUM_LOCK			0x11C
#define	HUMIN_KC_NUMPAD_SLASH			0x11D
#define	HUMIN_KC_NUMPAD_ASTERISK		0x11E
#define	HUMIN_KC_NUMPAD_HYPHEN			0x11F
#define	HUMIN_KC_DELETE				0x120
#define	HUMIN_KC_END				0x121
#define	HUMIN_KC_PAGE_DOWN			0x122
#define	HUMIN_KC_NUMPAD_7			0x123
#define	HUMIN_KC_NUMPAD_8			0x124
#define	HUMIN_KC_NUMPAD_9			0x125
#define	HUMIN_KC_NUMPAD_PLUS			0x126
#define	HUMIN_KC_CAPS_LOCK			0x127
#define	HUMIN_KC_NUMPAD_4			0x128
#define	HUMIN_KC_NUMPAD_5			0x129
#define	HUMIN_KC_NUMPAD_6			0x12A
#define	HUMIN_KC_NUMPAD_1			0x12B
#define	HUMIN_KC_NUMPAD_2			0x12C
#define	HUMIN_KC_NUMPAD_3			0x12D
#define	HUMIN_KC_NUMPAD_RETURN			0x13E
#define	HUMIN_KC_NUMPAD_ENTER			HUMIN_KC_NUMPAD_RETURN
#define	HUMIN_KC_LSUPER				0x13F		/* left "super" key */
#define	HUMIN_KC_RSUPER				0x140		/* right "super" key */
#define	HUMIN_KC_CONTEXT			0x141		/* context menu key */
#define	HUMIN_KC_NUMPAD_0			0x142
#define	HUMIN_KC_NUMPAD_DEL			0x143		/* numpad delete key */
#define	HUMIN_KC_MOUSE_LEFT			0x144		/* left mouse button */
#define	HUMIN_KC_MOUSE_MIDDLE			0x145		/* middle mouse button */
#define	HUMIN_KC_MOUSE_RIGHT			0x146		/* right mouse button */

/**
 * Describes an input event.
 */
typedef union
{
	/**
	 * Message type, one of the HUMIN_EV_* macros.
	 */
	int					type;
	
	/**
	 * For HUMIN_EV_BUTTON_UP and HUMIN_EV_BUTTON_DOWN.
	 */
	struct
	{
		int				type;
		int				scancode;
		int				keycode;
	} button;
} HuminEvent;

/**
 * Forms the input event queue.
 */
typedef struct HuminEvQueue_
{
	HuminEvent				ev;
	struct HuminEvQueue_*			next;
} HuminEvQueue;

/**
 * Describes a particular human input device.
 */
typedef struct
{
	/**
	 * The semaphore which controlls access to this device.
	 */
	Semaphore				sem;
	
	/**
	 * Event counter.
	 */
	Semaphore				evCount;
	
	/**
	 * A unique ID, used to derive the X in /dev/huminX.
	 */
	int					id;
	
	/**
	 * Human-readable device name, such as "PS/2 Keyboard and Mouse".
	 */
	char					devname[512];
	
	/**
	 * Event queue. The events are only queued while the device is open.
	 */
	HuminEvQueue*				first;
	HuminEvQueue*				last;
	
	/**
	 * The device file that represents this device, and its name.
	 */
	Inode*					inode;
	char*					iname;
	
	/**
	 * Whether or not the device is currently in use (opened by a process).
	 */
	int					inUse;
	
	/**
	 * This is true while the device still exists.
	 */
	int					active;
} HuminDevice;

HuminDevice*		huminCreateDevice(const char *devname);
void			huminDeleteDevice(HuminDevice *hudev);
void			huminPostEvent(HuminDevice *hudev, HuminEvent *ev);

#endif
