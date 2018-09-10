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

#include "codefield.h"

typedef struct
{
	/**
	 * Current language to highlight.
	 */
	LangRule*			rules;
} CodeData;

static int codeHandler(GWMEvent *ev, CodeField *code, void *context);

static DDIColor* getTokenColor(int type)
{
	switch (type)
	{
	case TOK_KEYWORD:
		return (DDIColor*) gwmGetThemeProp("gwm.syntax.keyword", GWM_TYPE_COLOR, NULL);
	case TOK_TYPE:
		return (DDIColor*) gwmGetThemeProp("gwm.syntax.type", GWM_TYPE_COLOR, NULL);
	case TOK_CONST:
		return (DDIColor*) gwmGetThemeProp("gwm.syntax.const", GWM_TYPE_COLOR, NULL);
	case TOK_NUMBER:
		return (DDIColor*) gwmGetThemeProp("gwm.syntax.number", GWM_TYPE_COLOR, NULL);
	case TOK_BUILTIN:
		return (DDIColor*) gwmGetThemeProp("gwm.syntax.builtin", GWM_TYPE_COLOR, NULL);
	case TOK_COMMENT:
		return (DDIColor*) gwmGetThemeProp("gwm.syntax.comment", GWM_TYPE_COLOR, NULL);
	case TOK_STRING:
		return (DDIColor*) gwmGetThemeProp("gwm.syntax.string", GWM_TYPE_COLOR, NULL);
	case TOK_PREPROC:
		return (DDIColor*) gwmGetThemeProp("gwm.syntax.preproc", GWM_TYPE_COLOR, NULL);
	default:
		return NULL;
	};
};

static void colorToken(CodeField *code, size_t pos, char *token, LangRule *rules, int type)
{
	if (type != -1)
	{
		if (type != TOK_BLANK)
		{
			DDIColor *color = getTokenColor(type);
			gwmSetTextFieldColorRange(code, pos, pos+ddiCountUTF8(token), color);
		};
	}
	else
	{
		// parse
		char *scan = token;
		while (*scan != 0)
		{
			// try to match this against a regex
			size_t matchLen;
			LangRule *rule;
			
			for (rule=rules; rule!=NULL; rule=rule->next)
			{
				ssize_t thisMatch = lexMatch(rule->regex, scan);
				if (thisMatch != -1)
				{
					matchLen = (size_t) thisMatch;
					break;
				};
			};
			
			if (rule == NULL || matchLen == 0)
			{
				// nothing to see here
				scan = (char*) ddiGetOffsetUTF8(scan, 1);
				pos++;
			}
			else
			{
				// split
				char *subtok = (char*) malloc(matchLen+1);
				memcpy(subtok, scan, matchLen);
				subtok[matchLen] = 0;
				
				scan += matchLen;
				size_t tokLenUTF8 = ddiCountUTF8(subtok);
				
				if (rule->subctx == NULL)
				{
					colorToken(code, pos, subtok, NULL, rule->type);
				}
				else
				{
					colorToken(code, pos, subtok, rule->subctx, -1);
				};
				
				pos += tokLenUTF8;
			};
		};
	};
	
	free(token);
};

static void recolorCode(CodeField *code)
{
	CodeData *data = (CodeData*) gwmGetData(code, codeHandler);
	
	const char *text = gwmReadTextField(code);
	gwmClearTextFieldStyles(code);
	
	colorToken(code, 0, strdup(text), data->rules, -1);
};

static int codeHandler(GWMEvent *ev, CodeField *code, void *context)
{
	switch (ev->type)
	{
	case GWM_EVENT_VALUE_CHANGED:
		recolorCode(code);
		return GWM_EVSTATUS_CONT;
	default:
		return GWM_EVSTATUS_CONT;
	};
};

CodeField* NewCodeField(GWMWindow *parent)
{
	CodeField *code = gwmNewTextField(parent);
	
	CodeData *data = (CodeData*) malloc(sizeof(CodeData));
	data->rules = NULL;
	
	gwmPushEventHandler(code, codeHandler, data);
	return code;
};

void DestroyCodeField(CodeField *code)
{
	CodeData *data = (CodeData*) gwmGetData(code, codeHandler);
	free(data);
	
	gwmDestroyTextField(code);
};

void SetCodeLang(CodeField *code, LangRule *rules)
{
	CodeData *data = (CodeData*) gwmGetData(code, codeHandler);
	data->rules = rules;
	recolorCode(code);
};
