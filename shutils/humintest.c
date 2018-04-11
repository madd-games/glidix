/*
	Glidix Shell Utilities

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

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <glidix/humin/humin.h>

typedef struct
{
	const char *name;
	int keycode;
} KeycodeName;

KeycodeName keycodeNames[] = {
	{"HUMIN_KC_NULL", HUMIN_KC_NULL},
	{"HUMIN_KC_ESC", HUMIN_KC_ESC},
	{"HUMIN_KC_BACKTICK", HUMIN_KC_BACKTICK},
	{"HUMIN_KC_1", HUMIN_KC_1},
	{"HUMIN_KC_2", HUMIN_KC_2},
	{"HUMIN_KC_3", HUMIN_KC_3},
	{"HUMIN_KC_4", HUMIN_KC_4},
	{"HUMIN_KC_5", HUMIN_KC_5},
	{"HUMIN_KC_6", HUMIN_KC_6},
	{"HUMIN_KC_7", HUMIN_KC_7},
	{"HUMIN_KC_8", HUMIN_KC_8},
	{"HUMIN_KC_9", HUMIN_KC_9},
	{"HUMIN_KC_0", HUMIN_KC_0},
	{"HUMIN_KC_HYPHEN", HUMIN_KC_HYPHEN},
	{"HUMIN_KC_EQUALS", HUMIN_KC_EQUALS},
	{"HUMIN_KC_BACKSPACE", HUMIN_KC_BACKSPACE},
	{"HUMIN_KC_TAB", HUMIN_KC_TAB},
	{"HUMIN_KC_Q", HUMIN_KC_Q},
	{"HUMIN_KC_W", HUMIN_KC_W},
	{"HUMIN_KC_E", HUMIN_KC_E},
	{"HUMIN_KC_R", HUMIN_KC_R},
	{"HUMIN_KC_T", HUMIN_KC_T},
	{"HUMIN_KC_Y", HUMIN_KC_Y},
	{"HUMIN_KC_U", HUMIN_KC_U},
	{"HUMIN_KC_I", HUMIN_KC_I},
	{"HUMIN_KC_O", HUMIN_KC_O},
	{"HUMIN_KC_P", HUMIN_KC_P},
	{"HUMIN_KC_SQB_LEFT", HUMIN_KC_SQB_LEFT},
	{"HUMIN_KC_SQB_RIGHT", HUMIN_KC_SQB_RIGHT},
	{"HUMIN_KC_RETURN", HUMIN_KC_RETURN},
	{"HUMIN_KC_ENTER", HUMIN_KC_ENTER},
	{"HUMIN_KC_A", HUMIN_KC_A},
	{"HUMIN_KC_S", HUMIN_KC_S},
	{"HUMIN_KC_D", HUMIN_KC_D},
	{"HUMIN_KC_F", HUMIN_KC_F},
	{"HUMIN_KC_G", HUMIN_KC_G},
	{"HUMIN_KC_H", HUMIN_KC_H},
	{"HUMIN_KC_J", HUMIN_KC_J},
	{"HUMIN_KC_K", HUMIN_KC_K},
	{"HUMIN_KC_L", HUMIN_KC_L},
	{"HUMIN_KC_SEMICOLON", HUMIN_KC_SEMICOLON},
	{"HUMIN_KC_QUOTE", HUMIN_KC_QUOTE},
	{"HUMIN_KC_BACKSLASH", HUMIN_KC_BACKSLASH},
	{"HUMIN_KC_ARROWS", HUMIN_KC_ARROWS},
	{"HUMIN_KC_Z", HUMIN_KC_Z},
	{"HUMIN_KC_X", HUMIN_KC_X},
	{"HUMIN_KC_C", HUMIN_KC_C},
	{"HUMIN_KC_V", HUMIN_KC_V},
	{"HUMIN_KC_B", HUMIN_KC_B},
	{"HUMIN_KC_N", HUMIN_KC_N},
	{"HUMIN_KC_M", HUMIN_KC_M},
	{"HUMIN_KC_COMMA", HUMIN_KC_COMMA},
	{"HUMIN_KC_PERIOD", HUMIN_KC_PERIOD},
	{"HUMIN_KC_SLASH", HUMIN_KC_SLASH},
	{"HUMIN_KC_SPACE", HUMIN_KC_SPACE},
	{"HUMIN_KC_LEFT", HUMIN_KC_LEFT},
	{"HUMIN_KC_RIGHT", HUMIN_KC_RIGHT},
	{"HUMIN_KC_UP", HUMIN_KC_UP},
	{"HUMIN_KC_DOWN", HUMIN_KC_DOWN},
	{"HUMIN_KC_LCTRL", HUMIN_KC_LCTRL},
	{"HUMIN_KC_LSHIFT", HUMIN_KC_LSHIFT},
	{"HUMIN_KC_LALT", HUMIN_KC_LALT},
	{"HUMIN_KC_RCTRL", HUMIN_KC_RCTRL},
	{"HUMIN_KC_RSHIFT", HUMIN_KC_RSHIFT},
	{"HUMIN_KC_RALT", HUMIN_KC_RALT},
	{"HUMIN_KC_F1", HUMIN_KC_F1},
	{"HUMIN_KC_F2", HUMIN_KC_F2},
	{"HUMIN_KC_F3", HUMIN_KC_F3},
	{"HUMIN_KC_F4", HUMIN_KC_F4},
	{"HUMIN_KC_F5", HUMIN_KC_F5},
	{"HUMIN_KC_F6", HUMIN_KC_F6},
	{"HUMIN_KC_F7", HUMIN_KC_F7},
	{"HUMIN_KC_F8", HUMIN_KC_F8},
	{"HUMIN_KC_F9", HUMIN_KC_F9},
	{"HUMIN_KC_F10", HUMIN_KC_F10},
	{"HUMIN_KC_F11", HUMIN_KC_F11},
	{"HUMIN_KC_F12", HUMIN_KC_F12},
	{"HUMIN_KC_PRINT_SCREEN", HUMIN_KC_PRINT_SCREEN},
	{"HUMIN_KC_SCROLL_LOCK", HUMIN_KC_SCROLL_LOCK},
	{"HUMIN_KC_PAUSE", HUMIN_KC_PAUSE},
	{"HUMIN_KC_INSERT", HUMIN_KC_INSERT},
	{"HUMIN_KC_HOME", HUMIN_KC_HOME},
	{"HUMIN_KC_PAGE_UP", HUMIN_KC_PAGE_UP},
	{"HUMIN_KC_NUM_LOCK", HUMIN_KC_NUM_LOCK},
	{"HUMIN_KC_NUMPAD_SLASH", HUMIN_KC_NUMPAD_SLASH},
	{"HUMIN_KC_NUMPAD_ASTERISK", HUMIN_KC_NUMPAD_ASTERISK},
	{"HUMIN_KC_NUMPAD_HYPHEN", HUMIN_KC_NUMPAD_HYPHEN},
	{"HUMIN_KC_DELETE", HUMIN_KC_DELETE},
	{"HUMIN_KC_END", HUMIN_KC_END},
	{"HUMIN_KC_PAGE_DOWN", HUMIN_KC_PAGE_DOWN},
	{"HUMIN_KC_NUMPAD_7", HUMIN_KC_NUMPAD_7},
	{"HUMIN_KC_NUMPAD_8", HUMIN_KC_NUMPAD_8},
	{"HUMIN_KC_NUMPAD_9", HUMIN_KC_NUMPAD_9},
	{"HUMIN_KC_NUMPAD_PLUS", HUMIN_KC_NUMPAD_PLUS},
	{"HUMIN_KC_CAPS_LOCK", HUMIN_KC_CAPS_LOCK},
	{"HUMIN_KC_NUMPAD_4", HUMIN_KC_NUMPAD_4},
	{"HUMIN_KC_NUMPAD_5", HUMIN_KC_NUMPAD_5},
	{"HUMIN_KC_NUMPAD_6", HUMIN_KC_NUMPAD_6},
	{"HUMIN_KC_NUMPAD_1", HUMIN_KC_NUMPAD_1},
	{"HUMIN_KC_NUMPAD_2", HUMIN_KC_NUMPAD_2},
	{"HUMIN_KC_NUMPAD_3", HUMIN_KC_NUMPAD_3},
	{"HUMIN_KC_NUMPAD_RETURN", HUMIN_KC_NUMPAD_RETURN},
	{"HUMIN_KC_NUMPAD_ENTER", HUMIN_KC_NUMPAD_ENTER},
	{"HUMIN_KC_LSUPER", HUMIN_KC_LSUPER},
	{"HUMIN_KC_RSUPER", HUMIN_KC_RSUPER},
	{"HUMIN_KC_CONTEXT", HUMIN_KC_CONTEXT},
	{"HUMIN_KC_NUMPAD_0", HUMIN_KC_NUMPAD_0},
	{"HUMIN_KC_NUMPAD_DEL", HUMIN_KC_NUMPAD_DEL},
	{"HUMIN_KC_MOUSE_LEFT", HUMIN_KC_MOUSE_LEFT},
	{"HUMIN_KC_MOUSE_MIDDLE", HUMIN_KC_MOUSE_MIDDLE},
	{"HUMIN_KC_MOUSE_RIGHT", HUMIN_KC_MOUSE_RIGHT},
	{NULL, 0}
};

const char *getKeycodeName(int keycode)
{
	KeycodeName *mapping;
	for (mapping=keycodeNames; mapping->name!=NULL; mapping++)
	{
		if (mapping->keycode == keycode) return mapping->name;
	};
	
	return "<unknown>";
};

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		fprintf(stderr, "USAGE:\t%s <device>\n", argv[0]);
		fprintf(stderr, "\tTest a human input device.\n");
		return 1;
	};
	
	int fd = open(argv[1], O_RDWR);
	if (fd == -1)
	{
		fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], argv[1], strerror(errno));
		return 1;
	};
	
	HuminEvent hev;
	while (1)
	{
		ssize_t sz = read(fd, &hev, sizeof(HuminEvent));
		if (sz == -1) perror("read");
		else
		{
			if (hev.type == HUMIN_EV_BUTTON_UP)
			{
				printf("[UP  ] scancode=0x%04X  keycode=0x%03X (%s)\n",
					(unsigned)hev.button.scancode,
					(unsigned)hev.button.keycode,
					getKeycodeName(hev.button.keycode));
			}
			else if (hev.type == HUMIN_EV_BUTTON_DOWN)
			{
				printf("[DOWN] scancode=0x%04X  keycode=0x%03X (%s)\n",
					(unsigned)hev.button.scancode,
					(unsigned)hev.button.keycode,
					getKeycodeName(hev.button.keycode));
			}
			else
			{
				printf("unknown event %d\n", hev.type);
			};
		};
	};
	
	close(fd);
	return 0;
};
