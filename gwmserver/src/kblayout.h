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

#ifndef KBLAYOUT_H
#define KBLAYOUT_H

#include <inttypes.h>

/**
 * Keyboard directives (used in scancode rules).
 */
enum
{
	KD_KEYCODE,
	KD_KEYCHAR,
	KD_MODIFIER,
	KD_TOGGLE
};

/**
 * Keyboard directive. An array of those forms a rule.
 */
typedef struct
{
	/**
	 * Parameter to the directive.
	 */
	uint64_t par;
	
	/**
	 * Condition. All bits set in this field must also be set in the "keymod" variable, else this
	 * directive is ignored.
	 */
	int cond;
	
	/**
	 * The directive type (KD_*).
	 */
	int type;
} KBL_Directive;

/**
 * A keyboard rule. Specifies what to do with a keycode.
 */
typedef struct
{
	/**
	 * List of directives.
	 */
	KBL_Directive *dirs;
	size_t numDirs;
} KBL_Rule;

/**
 * Describes a keyboard layout.
 */
typedef struct
{
	/**
	 * Table of rules. Valid keycodes are 0x000-0x1FF
	 */
	KBL_Rule rules[0x200];
} KeyboardLayout;

/**
 * Load a keyboard layout file. Write any error/warning messages to the specified file. Returns 0 on success,
 * or -1 on error; if it fails, the keyboard layout remains unchanged.
 */
int kblSet(const char *filename, FILE *errfp);

/**
 * Translate a keycode to a keychar (and/or another keycode) according to the current keyboard layout.
 * 'evtype' is the type of input event (GWM_EVENT_UP or GWM_EVENT_DOWN) and is needed to control keymod.
 */
void kblTranslate(int *keycode, uint64_t *keychar, int *keymod, int evtype);

#endif
