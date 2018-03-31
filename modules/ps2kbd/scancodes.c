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

#include <glidix/humin/humin.h>

/**
 * 0 = main page
 * 1 = extended page
 */
int scCurrentPage = 0;

/**
 * Main page of scancodes (without the 0xE0 prefix). Scancode set 1 is used throughout.
 */
int scPageMain[128] = {
	HUMIN_KC_NULL,			/* 0x00 */
	HUMIN_KC_ESC,			/* 0x01 */
	HUMIN_KC_1,			/* 0x02 */
	HUMIN_KC_2,			/* 0x03 */
	HUMIN_KC_3,			/* 0x04 */
	HUMIN_KC_4,			/* 0x05 */
	HUMIN_KC_5,			/* 0x06 */
	HUMIN_KC_6,			/* 0x07 */
	HUMIN_KC_7,			/* 0x08 */
	HUMIN_KC_8,			/* 0x09 */
	HUMIN_KC_9,			/* 0x0A */
	HUMIN_KC_0,			/* 0x0B */
	HUMIN_KC_HYPHEN,		/* 0x0C */
	HUMIN_KC_EQUALS,		/* 0x0D */
	HUMIN_KC_BACKSPACE,		/* 0x0E */
	HUMIN_KC_TAB,			/* 0x0F */
	HUMIN_KC_Q,			/* 0x10 */
	HUMIN_KC_W,			/* 0x11 */
	HUMIN_KC_E,			/* 0x12 */
	HUMIN_KC_R,			/* 0x13 */
	HUMIN_KC_T,			/* 0x14 */
	HUMIN_KC_Y,			/* 0x15 */
	HUMIN_KC_U,			/* 0x16 */
	HUMIN_KC_I,			/* 0x17 */
	HUMIN_KC_O,			/* 0x18 */
	HUMIN_KC_P,			/* 0x19 */
	HUMIN_KC_SQB_LEFT,		/* 0x1A */
	HUMIN_KC_SQB_RIGHT,		/* 0x1B */
	HUMIN_KC_RETURN,		/* 0x1C */
	HUMIN_KC_LCTRL,			/* 0x1D */
	HUMIN_KC_A,			/* 0x1E */
	HUMIN_KC_S,			/* 0x1F */
	HUMIN_KC_D,			/* 0x20 */
	HUMIN_KC_F,			/* 0x21 */
	HUMIN_KC_G,			/* 0x22 */
	HUMIN_KC_H,			/* 0x23 */
	HUMIN_KC_J,			/* 0x24 */
	HUMIN_KC_K,			/* 0x25 */
	HUMIN_KC_L,			/* 0x26 */
	HUMIN_KC_SEMICOLON,		/* 0x27 */
	HUMIN_KC_QUOTE,			/* 0x28 */
	HUMIN_KC_BACKTICK,		/* 0x29 */
	HUMIN_KC_LSHIFT,		/* 0x2A */
	HUMIN_KC_BACKSLASH,		/* 0x2B */
	HUMIN_KC_Z,			/* 0x2C */
	HUMIN_KC_X,			/* 0x2D */
	HUMIN_KC_C,			/* 0x2E */
	HUMIN_KC_V,			/* 0x2F */
	HUMIN_KC_B,			/* 0x30 */
	HUMIN_KC_N,			/* 0x31 */
	HUMIN_KC_M,			/* 0x32 */
	HUMIN_KC_COMMA,			/* 0x33 */
	HUMIN_KC_PERIOD,		/* 0x34 */
	HUMIN_KC_SLASH,			/* 0x35 */
	HUMIN_KC_RSHIFT,		/* 0x36 */
	HUMIN_KC_NUMPAD_ASTERISK,	/* 0x37 */
	HUMIN_KC_LALT,			/* 0x38 */
	HUMIN_KC_SPACE,			/* 0x39 */
	HUMIN_KC_CAPS_LOCK,		/* 0x3A */
	HUMIN_KC_F1,			/* 0x3B */
	HUMIN_KC_F2,			/* 0x3C */
	HUMIN_KC_F3,			/* 0x3D */
	HUMIN_KC_F4,			/* 0x3E */
	HUMIN_KC_F5,			/* 0x3F */
	HUMIN_KC_F6,			/* 0x40 */
	HUMIN_KC_F7,			/* 0x41 */
	HUMIN_KC_F8,			/* 0x42 */
	HUMIN_KC_F9,			/* 0x43 */
	HUMIN_KC_F10,			/* 0x44 */
	HUMIN_KC_NUM_LOCK,		/* 0x45 */
	HUMIN_KC_SCROLL_LOCK,		/* 0x46 */
	HUMIN_KC_NUMPAD_7,		/* 0x47 */
	HUMIN_KC_NUMPAD_8,		/* 0x48 */
	HUMIN_KC_NUMPAD_9,		/* 0x49 */
	HUMIN_KC_NUMPAD_HYPHEN,		/* 0x4A */
	HUMIN_KC_NUMPAD_4,		/* 0x4B */
	HUMIN_KC_NUMPAD_5,		/* 0x4C */
	HUMIN_KC_NUMPAD_6,		/* 0x4D */
	HUMIN_KC_NUMPAD_PLUS,		/* 0x4E */
	HUMIN_KC_NUMPAD_1,		/* 0x4F */
	HUMIN_KC_NUMPAD_2,		/* 0x50 */
	HUMIN_KC_NUMPAD_3,		/* 0x51 */
	HUMIN_KC_NUMPAD_0,		/* 0x52 */
	HUMIN_KC_NUMPAD_DEL,		/* 0x53 */
	HUMIN_KC_NULL,			/* 0x54 */
	HUMIN_KC_NULL,			/* 0x55 */
	HUMIN_KC_ARROWS,		/* 0x56 */
	HUMIN_KC_F11,			/* 0x57 */
	HUMIN_KC_F12,			/* 0x58 */
	HUMIN_KC_NULL,			/* 0x59 */
	HUMIN_KC_NULL,			/* 0x5A */
	HUMIN_KC_NULL,			/* 0x5B */
	HUMIN_KC_NULL,			/* 0x5C */
	HUMIN_KC_NULL,			/* 0x5D */
	HUMIN_KC_NULL,			/* 0x5E */
	HUMIN_KC_NULL,			/* 0x5F */
	HUMIN_KC_NULL,			/* 0x60 */
	HUMIN_KC_NULL,			/* 0x61 */
	HUMIN_KC_NULL,			/* 0x62 */
	HUMIN_KC_NULL,			/* 0x63 */
	HUMIN_KC_NULL,			/* 0x64 */
	HUMIN_KC_NULL,			/* 0x65 */
	HUMIN_KC_NULL,			/* 0x66 */
	HUMIN_KC_NULL,			/* 0x67 */
	HUMIN_KC_NULL,			/* 0x68 */
	HUMIN_KC_NULL,			/* 0x69 */
	HUMIN_KC_NULL,			/* 0x6A */
	HUMIN_KC_NULL,			/* 0x6B */
	HUMIN_KC_NULL,			/* 0x6C */
	HUMIN_KC_NULL,			/* 0x6D */
	HUMIN_KC_NULL,			/* 0x6E */
	HUMIN_KC_NULL,			/* 0x6F */
	HUMIN_KC_NULL,			/* 0x70 */
	HUMIN_KC_NULL,			/* 0x71 */
	HUMIN_KC_NULL,			/* 0x72 */
	HUMIN_KC_NULL,			/* 0x73 */
	HUMIN_KC_NULL,			/* 0x74 */
	HUMIN_KC_NULL,			/* 0x75 */
	HUMIN_KC_NULL,			/* 0x76 */
	HUMIN_KC_NULL,			/* 0x77 */
	HUMIN_KC_NULL,			/* 0x78 */
	HUMIN_KC_NULL,			/* 0x79 */
	HUMIN_KC_NULL,			/* 0x7A */
	HUMIN_KC_NULL,			/* 0x7B */
	HUMIN_KC_NULL,			/* 0x7C */
	HUMIN_KC_NULL,			/* 0x7D */
	HUMIN_KC_NULL,			/* 0x7E */
	HUMIN_KC_NULL,			/* 0x7F */
};

/**
 * Extended scancodes (with 0xE0 prefix). Scancode set 1 is used throughout.
 */
int scPageExt[128] = {
	HUMIN_KC_NULL,			/* 0xE0, 0x00 */
	HUMIN_KC_NULL,			/* 0xE0, 0x01 */
	HUMIN_KC_NULL,			/* 0xE0, 0x02 */
	HUMIN_KC_NULL,			/* 0xE0, 0x03 */
	HUMIN_KC_NULL,			/* 0xE0, 0x04 */
	HUMIN_KC_NULL,			/* 0xE0, 0x05 */
	HUMIN_KC_NULL,			/* 0xE0, 0x06 */
	HUMIN_KC_NULL,			/* 0xE0, 0x07 */
	HUMIN_KC_NULL,			/* 0xE0, 0x08 */
	HUMIN_KC_NULL,			/* 0xE0, 0x09 */
	HUMIN_KC_NULL,			/* 0xE0, 0x0A */
	HUMIN_KC_NULL,			/* 0xE0, 0x0B */
	HUMIN_KC_NULL,			/* 0xE0, 0x0C */
	HUMIN_KC_NULL,			/* 0xE0, 0x0D */
	HUMIN_KC_NULL,			/* 0xE0, 0x0E */
	HUMIN_KC_NULL,			/* 0xE0, 0x0F */
	HUMIN_KC_NULL,			/* 0xE0, 0x10 */
	HUMIN_KC_NULL,			/* 0xE0, 0x11 */
	HUMIN_KC_NULL,			/* 0xE0, 0x12 */
	HUMIN_KC_NULL,			/* 0xE0, 0x13 */
	HUMIN_KC_NULL,			/* 0xE0, 0x14 */
	HUMIN_KC_NULL,			/* 0xE0, 0x15 */
	HUMIN_KC_NULL,			/* 0xE0, 0x16 */
	HUMIN_KC_NULL,			/* 0xE0, 0x17 */
	HUMIN_KC_NULL,			/* 0xE0, 0x18 */
	HUMIN_KC_NULL,			/* 0xE0, 0x19 */
	HUMIN_KC_NULL,			/* 0xE0, 0x1A */
	HUMIN_KC_NULL,			/* 0xE0, 0x1B */
	HUMIN_KC_NUMPAD_RETURN,		/* 0xE0, 0x1C */
	HUMIN_KC_RCTRL,			/* 0xE0, 0x1D */
	HUMIN_KC_NULL,			/* 0xE0, 0x1E */
	HUMIN_KC_NULL,			/* 0xE0, 0x1F */
	HUMIN_KC_NULL,			/* 0xE0, 0x20 */
	HUMIN_KC_NULL,			/* 0xE0, 0x21 */
	HUMIN_KC_NULL,			/* 0xE0, 0x22 */
	HUMIN_KC_NULL,			/* 0xE0, 0x23 */
	HUMIN_KC_NULL,			/* 0xE0, 0x24 */
	HUMIN_KC_NULL,			/* 0xE0, 0x25 */
	HUMIN_KC_NULL,			/* 0xE0, 0x26 */
	HUMIN_KC_NULL,			/* 0xE0, 0x27 */
	HUMIN_KC_NULL,			/* 0xE0, 0x28 */
	HUMIN_KC_NULL,			/* 0xE0, 0x29 */
	HUMIN_KC_NULL,			/* 0xE0, 0x2A */
	HUMIN_KC_NULL,			/* 0xE0, 0x2B */
	HUMIN_KC_NULL,			/* 0xE0, 0x2C */
	HUMIN_KC_NULL,			/* 0xE0, 0x2D */
	HUMIN_KC_NULL,			/* 0xE0, 0x2E */
	HUMIN_KC_NULL,			/* 0xE0, 0x2F */
	HUMIN_KC_NULL,			/* 0xE0, 0x30 */
	HUMIN_KC_NULL,			/* 0xE0, 0x31 */
	HUMIN_KC_NULL,			/* 0xE0, 0x32 */
	HUMIN_KC_NULL,			/* 0xE0, 0x33 */
	HUMIN_KC_NULL,			/* 0xE0, 0x34 */
	HUMIN_KC_NUMPAD_SLASH,		/* 0xE0, 0x35 */
	HUMIN_KC_NULL,			/* 0xE0, 0x36 */
	HUMIN_KC_NULL,			/* 0xE0, 0x37 */
	HUMIN_KC_RALT,			/* 0xE0, 0x38 */
	HUMIN_KC_NULL,			/* 0xE0, 0x39 */
	HUMIN_KC_NULL,			/* 0xE0, 0x3A */
	HUMIN_KC_NULL,			/* 0xE0, 0x3B */
	HUMIN_KC_NULL,			/* 0xE0, 0x3C */
	HUMIN_KC_NULL,			/* 0xE0, 0x3D */
	HUMIN_KC_NULL,			/* 0xE0, 0x3E */
	HUMIN_KC_NULL,			/* 0xE0, 0x3F */
	HUMIN_KC_NULL,			/* 0xE0, 0x40 */
	HUMIN_KC_NULL,			/* 0xE0, 0x41 */
	HUMIN_KC_NULL,			/* 0xE0, 0x42 */
	HUMIN_KC_NULL,			/* 0xE0, 0x43 */
	HUMIN_KC_NULL,			/* 0xE0, 0x44 */
	HUMIN_KC_NULL,			/* 0xE0, 0x45 */
	HUMIN_KC_NULL,			/* 0xE0, 0x46 */
	HUMIN_KC_HOME,			/* 0xE0, 0x47 */
	HUMIN_KC_UP,			/* 0xE0, 0x48 */
	HUMIN_KC_PAGE_UP,		/* 0xE0, 0x49 */
	HUMIN_KC_NULL,			/* 0xE0, 0x4A */
	HUMIN_KC_LEFT,			/* 0xE0, 0x4B */
	HUMIN_KC_NULL,			/* 0xE0, 0x4C */
	HUMIN_KC_RIGHT,			/* 0xE0, 0x4D */
	HUMIN_KC_NULL,			/* 0xE0, 0x4E */
	HUMIN_KC_END,			/* 0xE0, 0x4F */
	HUMIN_KC_DOWN,			/* 0xE0, 0x50 */
	HUMIN_KC_PAGE_DOWN,		/* 0xE0, 0x51 */
	HUMIN_KC_INSERT,		/* 0xE0, 0x52 */
	HUMIN_KC_DELETE,		/* 0xE0, 0x53 */
	HUMIN_KC_NULL,			/* 0xE0, 0x54 */
	HUMIN_KC_NULL,			/* 0xE0, 0x55 */
	HUMIN_KC_NULL,			/* 0xE0, 0x56 */
	HUMIN_KC_NULL,			/* 0xE0, 0x57 */
	HUMIN_KC_NULL,			/* 0xE0, 0x58 */
	HUMIN_KC_NULL,			/* 0xE0, 0x59 */
	HUMIN_KC_NULL,			/* 0xE0, 0x5A */
	HUMIN_KC_LSUPER,		/* 0xE0, 0x5B */
	HUMIN_KC_RSUPER,		/* 0xE0, 0x5C */
	HUMIN_KC_CONTEXT,		/* 0xE0, 0x5D */
	HUMIN_KC_NULL,			/* 0xE0, 0x5E */
	HUMIN_KC_NULL,			/* 0xE0, 0x5F */
	HUMIN_KC_NULL,			/* 0xE0, 0x60 */
	HUMIN_KC_NULL,			/* 0xE0, 0x61 */
	HUMIN_KC_NULL,			/* 0xE0, 0x62 */
	HUMIN_KC_NULL,			/* 0xE0, 0x63 */
	HUMIN_KC_NULL,			/* 0xE0, 0x64 */
	HUMIN_KC_NULL,			/* 0xE0, 0x65 */
	HUMIN_KC_NULL,			/* 0xE0, 0x66 */
	HUMIN_KC_NULL,			/* 0xE0, 0x67 */
	HUMIN_KC_NULL,			/* 0xE0, 0x68 */
	HUMIN_KC_NULL,			/* 0xE0, 0x69 */
	HUMIN_KC_NULL,			/* 0xE0, 0x6A */
	HUMIN_KC_NULL,			/* 0xE0, 0x6B */
	HUMIN_KC_NULL,			/* 0xE0, 0x6C */
	HUMIN_KC_NULL,			/* 0xE0, 0x6D */
	HUMIN_KC_NULL,			/* 0xE0, 0x6E */
	HUMIN_KC_NULL,			/* 0xE0, 0x6F */
	HUMIN_KC_NULL,			/* 0xE0, 0x70 */
	HUMIN_KC_NULL,			/* 0xE0, 0x71 */
	HUMIN_KC_NULL,			/* 0xE0, 0x72 */
	HUMIN_KC_NULL,			/* 0xE0, 0x73 */
	HUMIN_KC_NULL,			/* 0xE0, 0x74 */
	HUMIN_KC_NULL,			/* 0xE0, 0x75 */
	HUMIN_KC_NULL,			/* 0xE0, 0x76 */
	HUMIN_KC_NULL,			/* 0xE0, 0x77 */
	HUMIN_KC_NULL,			/* 0xE0, 0x78 */
	HUMIN_KC_NULL,			/* 0xE0, 0x79 */
	HUMIN_KC_NULL,			/* 0xE0, 0x7A */
	HUMIN_KC_NULL,			/* 0xE0, 0x7B */
	HUMIN_KC_NULL,			/* 0xE0, 0x7C */
	HUMIN_KC_NULL,			/* 0xE0, 0x7D */
	HUMIN_KC_NULL,			/* 0xE0, 0x7E */
	HUMIN_KC_NULL,			/* 0xE0, 0x7F */
};

void scancodeGotoExt()
{
	scCurrentPage = 1;
};

int scancodeTranslate(uint8_t scancode)
{
	scancode &= 0x7F;
	
	if (scCurrentPage == 1)
	{
		scCurrentPage = 0;
		return scPageExt[scancode];
	}
	else
	{
		return scPageMain[scancode];
	};
};

int scancodeTransform(uint8_t scancode)
{
	int result = (int) scancode;
	if (scCurrentPage == 1) result |= 0xE000;
	return result;
};
