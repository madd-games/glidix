/*
	Glidix kernel

	Copyright (c) 2014-2015, Madd Games.
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

#ifndef __glidix_ktty_h
#define __glidix_ktty_h

/**
 * Provides file descriptions for stdin, stdout and stderr of the initial process,
 * so that it can write to the kernel console.
 */

#include <glidix/ftab.h>

/**
 * Start with 33 (the '!' sign) to make as many sequences as possible ASCII characters.
 */
#define	ESC_CLEAR_SCREEN			33
#define	ESC_SET_COLOR				34
#define	ESC_SET_CURSOR				35

/**
 * This union represents different kinds of escape sequences for the terminal.
 */
typedef union
{
	struct
	{
		char				escape;			/* '\e' */
		uint8_t				command;		/* start with '!' */
	} head;
	
	struct
	{
		uint16_t			ign;
		uint8_t				attr;			/* background/foreground */
	} setcolor;
	
	struct
	{
		uint16_t			ign;
		uint8_t				x;
		uint8_t				y;
	} setcursor;
} EscapeSequence;

void setupTerminal(FileTable *ftab);
void termPutChar(char c);

#endif
