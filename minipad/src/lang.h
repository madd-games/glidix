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

#ifndef LANG_H_
#define LANG_H_

#include <libgwm.h>

#include "lex.h"

#define	TOK_BLANK				0
#define	TOK_KEYWORD				1
#define	TOK_TYPE				2
#define	TOK_CONST				3
#define	TOK_NUMBER				4
#define	TOK_BUILTIN				5
#define	TOK_COMMENT				6
#define	TOK_STRING				7
#define	TOK_PREPROC				8

/**
 * Language rule. Specified a regular expression, which when matched at the beginning of a string, brings
 * the parser into a specific context with that string.
 */
typedef struct LangRule_
{
	struct LangRule_*			next;
	Regex*					regex;
	struct LangRule_*			subctx;		// list of sub-rules (context) or NULL
	int					type;		// token type to output if subctx == NULL
} LangRule;

LangRule* loadLangs(GWMOptionMenu *optLang, const char *mimename);

#endif
