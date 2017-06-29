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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgwm.h>

#include "kblayout.h"

#define	KBL_SEPARATORS	" \t"

/**
 * Numbering which construct we are inside.
 */
enum
{
	IN_NOTHING,
	IN_LAYOUT,
	IN_KEYCODE
};

/**
 * Maps flag names to masks.
 */
typedef struct FlagName_
{
	struct FlagName_*	next;
	
	/**
	 * Name (on the heap; call free() on this).
	 */
	char*			name;
	
	/**
	 * The bitmask.
	 */
	int			mask;
} FlagName;

pthread_mutex_t kblLock = PTHREAD_MUTEX_INITIALIZER;
KeyboardLayout *kblCurrent = NULL;
int kblCurrentMod = 0;

static int assertNext(FILE *errfp, const char *filename, int lineno, const char *expecting)
{
	char *tok = strtok(NULL, KBL_SEPARATORS);
	if (tok == NULL)
	{
		fprintf(errfp, "%s:%d: error: expecting '%s'\n", filename, lineno, expecting);
		return -1;
	};
	
	if (strcmp(tok, expecting) != 0)
	{
		fprintf(errfp, "%s:%d: error: expecting '%s'\n", filename, lineno, expecting);
		return -1;
	};
	
	return 0;
};

static int assertEnd(FILE *errfp, const char *filename, int lineno)
{
	if (strtok(NULL, KBL_SEPARATORS) != NULL)
	{
		fprintf(errfp, "%s:%d: error: expected end of line\n", filename, lineno);
		return -1;
	};
	
	return 0;
};

static void kblDelete(KeyboardLayout *kbl, FlagName *flagNames)
{
	while (flagNames != NULL)
	{
		FlagName *flag = flagNames;
		flagNames = flag->next;
		
		free(flag->name);
		free(flag);
	};
	
	free(kbl);
};

static FlagName* fnAdd(FlagName *flagNames, const char *name, int mask)
{
	FlagName *newFlag = (FlagName*) malloc(sizeof(FlagName));
	newFlag->name = strdup(name);
	newFlag->mask = mask;
	newFlag->next = flagNames;
	
	return newFlag;
};

static int fnLookup(FlagName *list, const char *name)
{
	for (; list!=NULL; list=list->next)
	{
		if (strcmp(list->name, name) == 0) return list->mask;
	};
	
	return -1;
};

int kblSet(const char *filename, FILE *errfp)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(errfp, "%s:0: cannot open file: %s\n", filename, strerror(errno));
		return -1;
	};
	
	KeyboardLayout *kbl = (KeyboardLayout*) malloc(sizeof(KeyboardLayout));
	memset(kbl, 0, sizeof(KeyboardLayout));
	FlagName *flagNames = NULL;
	flagNames = fnAdd(flagNames, "gwm_ctrl", GWM_KM_CTRL);
	flagNames = fnAdd(flagNames, "gwm_shift", GWM_KM_SHIFT);
	flagNames = fnAdd(flagNames, "gwm_alt", GWM_KM_ALT);
	flagNames = fnAdd(flagNames, "gwm_caps_lock", GWM_KM_CAPS_LOCK);
	flagNames = fnAdd(flagNames, "gwm_num_lock", GWM_KM_NUM_LOCK);
	flagNames = fnAdd(flagNames, "gwm_scroll_lock", GWM_KM_SCROLL_LOCK);
	KBL_Rule *currentRule;
	
	// which construct we are inside of
	int inside = IN_NOTHING;
	int nextFlagBit = 16;
	
	char linebuf[1024];
	char *line;
	int lineno = 0;
	while ((line = fgets(linebuf, 1024, fp)) != NULL)
	{
		lineno++;
		
		char *newline = strchr(line, '\n');
		if (newline != NULL) *newline = 0;
		
		char *commentPos = strchr(line, '#');
		if (commentPos != NULL) *commentPos = 0;
		
		char *cmd = strtok(line, KBL_SEPARATORS);
		if (cmd == NULL) continue;
		
		if (inside == IN_NOTHING)
		{
			if (strcmp(cmd, "layout") == 0)
			{
				if (assertNext(errfp, filename, lineno, "{") == -1)
				{
					kblDelete(kbl, flagNames);
					return -1;
				};
				
				if (assertEnd(errfp, filename, lineno) == -1)
				{
					kblDelete(kbl, flagNames);
					return -1;
				};
			
				inside = IN_LAYOUT;
			}
			else
			{
				fprintf(errfp, "%s:%d: error: unknown kbl command '%s'\n", filename, lineno, cmd);
				kblDelete(kbl, flagNames);
				return -1;
			};
		}
		else if (inside == IN_LAYOUT)
		{
			if (strcmp(cmd, "}") == 0)
			{
				if (assertEnd(errfp, filename, lineno) == -1)
				{
					kblDelete(kbl, flagNames);
					return -1;
				};
				
				inside = IN_NOTHING;
			}
			else if (strcmp(cmd, "flag") == 0)
			{
				char *name = strtok(NULL, KBL_SEPARATORS);
				if (name == NULL)
				{
					fprintf(errfp, "%s:%d: error: expected flag name\n", filename, lineno);
					kblDelete(kbl, flagNames);
					return -1;
				};
				
				if (fnLookup(flagNames, name) != -1)
				{
					fprintf(errfp, "%s:%d: error: flag named '%s' redefined\n", filename, lineno, name);
					kblDelete(kbl, flagNames);
					return -1;
				};
				
				if (memcmp(name, "gwm_", 4) == 0)
				{
					fprintf(errfp, "%s:%d: error: flag names beginning with 'gwm_' are reserved for the implementation\n", filename, lineno);
					kblDelete(kbl, flagNames);
					return -1;
				};
				
				flagNames = fnAdd(flagNames, name, (1 << (nextFlagBit++)));
			}
			else if (strcmp(cmd, "keycode") == 0)
			{
				char *numstr = strtok(NULL, KBL_SEPARATORS);
				if (numstr == NULL)
				{
					fprintf(errfp, "%s:%d: error: expected keycode\n", filename, lineno);
					kblDelete(kbl, flagNames);
					return -1;
				};
				
				char *endptr;
				long keycode = strtol(numstr, &endptr, 0);
				if (*endptr != 0)
				{
					fprintf(errfp, "%s:%d: error: 'keycode' requires a numeric parameter\n", filename, lineno);
					kblDelete(kbl, flagNames);
					return -1;
				};
				
				if ((keycode < 0) || (keycode > 0x1FF))
				{
					fprintf(errfp, "%s:%d: error: keycodes must be in the range 0x000-0x1FF\n", filename, lineno);
					kblDelete(kbl, flagNames);
					return -1;
				};
				
				if (assertNext(errfp, filename, lineno, "{") == -1)
				{
					kblDelete(kbl, flagNames);
					return -1;
				};
				
				if (assertEnd(errfp, filename, lineno) == -1)
				{
					kblDelete(kbl, flagNames);
					return -1;
				};
				
				inside = IN_KEYCODE;
				currentRule = &kbl->rules[keycode];
			}
			else
			{
				fprintf(errfp, "%s:%d: error: unknown kbl command '%s' in 'layout' construct\n", filename, lineno, cmd);
				kblDelete(kbl, flagNames);
				return -1;
			};
		}
		else if (inside == IN_KEYCODE)
		{
			if (strcmp(cmd, "}") == 0)
			{
				if (assertEnd(errfp, filename, lineno) == -1)
				{
					kblDelete(kbl, flagNames);
					return -1;
				};
				
				inside = IN_LAYOUT;
			}
			else
			{
				// parse conditions first if any
				int cond = 0;
				
				if (strcmp(cmd, "if") == 0)
				{
					while (1)
					{
						char *name = strtok(NULL, KBL_SEPARATORS);
						if (name == NULL)
						{
							fprintf(errfp, "%s:%d: error: expecting flag name\n", filename, lineno);
							kblDelete(kbl, flagNames);
							return -1;
						};
						
						int mask = fnLookup(flagNames, name);
						if (mask == -1)
						{
							fprintf(errfp, "%s:%d: error: unknown flag name '%s\n", filename, lineno, name);
							kblDelete(kbl, flagNames);
							return -1;
						};
						
						cond |= mask;
						
						char *sep = strtok(NULL, KBL_SEPARATORS);
						if (sep == NULL)
						{
							fprintf(errfp, "%s:%d: error: expected 'and' or 'then'\n", filename, lineno);
							kblDelete(kbl, flagNames);
							return -1;
						};
						
						if (strcmp(sep, "and") == 0)
						{
							continue;
						}
						else if (strcmp(sep, "then") == 0)
						{
							cmd = strtok(NULL, KBL_SEPARATORS);
							if (cmd == NULL)
							{
								fprintf(errfp, "%s:%d: error: expecting statement\n", filename, lineno);
								kblDelete(kbl, flagNames);
								return -1;
							};
							
							break;
						}
						else
						{
							fprintf(errfp, "%s:%d: error: expected 'and' or 'then'\n", filename, lineno);
							kblDelete(kbl, flagNames);
							return -1;
						};
					};
				};
				
				// now the actual command
				if (strcmp(cmd, "keycode") == 0)
				{
					char *numstr = strtok(NULL, KBL_SEPARATORS);
					if (numstr == NULL)
					{
						fprintf(errfp, "%s:%d: error: expected a keycode\n", filename, lineno);
						kblDelete(kbl, flagNames);
						return -1;
					};
				
					char *endptr;
					long keycode = strtol(numstr, &endptr, 0);
					if (*endptr != 0)
					{
						fprintf(errfp, "%s:%d: error: 'keycode' requires a numeric parameter\n", filename, lineno);
						kblDelete(kbl, flagNames);
						return -1;
					};
					
					size_t index = currentRule->numDirs++;
					currentRule->dirs = (KBL_Directive*) realloc(currentRule->dirs,
								sizeof(KBL_Directive) * currentRule->numDirs);
					currentRule->dirs[index].cond = cond;
					currentRule->dirs[index].par = (uint64_t) keycode;
					currentRule->dirs[index].type = KD_KEYCODE;
				}
				else if (strcmp(cmd, "keychar") == 0)
				{
					char *numstr = strtok(NULL, KBL_SEPARATORS);
					if (numstr == NULL)
					{
						fprintf(errfp, "%s:%d: error: expected a codepoint\n", filename, lineno);
						kblDelete(kbl, flagNames);
						return -1;
					};

					uint64_t codepoint;
					int count;
					if (sscanf(numstr, "U+%lx%n", &codepoint, &count) < 1)
					{
						fprintf(errfp, "%s:%d: error: expected a codepoint\n", filename, lineno);
						kblDelete(kbl, flagNames);
						return -1;
					};
					
					if (count != strlen(numstr))
					{
						fprintf(errfp, "%s:%d: error: expected a codepoint\n", filename, lineno);
						kblDelete(kbl, flagNames);
						return -1;
					};
					
					size_t index = currentRule->numDirs++;
					currentRule->dirs = (KBL_Directive*) realloc(currentRule->dirs,
								sizeof(KBL_Directive) * currentRule->numDirs);
					currentRule->dirs[index].cond = cond;
					currentRule->dirs[index].par = codepoint;
					currentRule->dirs[index].type = KD_KEYCHAR;
				}
				else if (strcmp(cmd, "modifier") == 0)
				{
					char *name = strtok(NULL, KBL_SEPARATORS);
					if (name == NULL)
					{
						fprintf(errfp, "%s:%d: error: expecting flag name\n", filename, lineno);
						kblDelete(kbl, flagNames);
						return -1;
					};
					
					int mask = fnLookup(flagNames, name);
					if (mask == -1)
					{
						fprintf(errfp, "%s:%d: error: unknown flag name '%s\n", filename, lineno, name);
						kblDelete(kbl, flagNames);
						return -1;
					};
					
					size_t index = currentRule->numDirs++;
					currentRule->dirs = (KBL_Directive*) realloc(currentRule->dirs,
								sizeof(KBL_Directive) * currentRule->numDirs);
					currentRule->dirs[index].cond = cond;
					currentRule->dirs[index].par = (uint64_t) mask;
					currentRule->dirs[index].type = KD_MODIFIER;
				}
				else if (strcmp(cmd, "toggle") == 0)
				{
					char *name = strtok(NULL, KBL_SEPARATORS);
					if (name == NULL)
					{
						fprintf(errfp, "%s:%d: error: expecting flag name\n", filename, lineno);
						kblDelete(kbl, flagNames);
						return -1;
					};
					
					int mask = fnLookup(flagNames, name);
					if (mask == -1)
					{
						fprintf(errfp, "%s:%d: error: unknown flag name '%s\n", filename, lineno, name);
						kblDelete(kbl, flagNames);
						return -1;
					};
					
					size_t index = currentRule->numDirs++;
					currentRule->dirs = (KBL_Directive*) realloc(currentRule->dirs,
								sizeof(KBL_Directive) * currentRule->numDirs);
					currentRule->dirs[index].cond = cond;
					currentRule->dirs[index].par = (uint64_t) mask;
					currentRule->dirs[index].type = KD_TOGGLE;
				}
				else
				{
					fprintf(errfp, "%s:%d: error: unknown directive '%s'\n", filename, lineno, cmd);
					kblDelete(kbl, flagNames);
					return -1;
				};
			};
		};
	};
	
	fclose(fp);
	kblDelete(NULL, flagNames);
	
	pthread_mutex_lock(&kblLock);
	if (kblCurrent != NULL) free(kblCurrent);
	kblCurrent = kbl;
	pthread_mutex_unlock(&kblLock);
	
	return 0;
};

void kblTranslate(int *keycode, uint64_t *keychar, int *keymod, int evtype)
{
	if (((*keycode) < 0) || ((*keycode) >= 0x1FF))
	{
		// invalid keycode for some reason
		return;
	};
	
	pthread_mutex_lock(&kblLock);
	if (kblCurrent == NULL)
	{
		pthread_mutex_unlock(&kblLock);
		return;
	};
	
	KBL_Rule *rule = &kblCurrent->rules[*keycode];
	size_t i;
	for (i=0; i<rule->numDirs; i++)
	{
		KBL_Directive *dir = &rule->dirs[i];
		if ((kblCurrentMod & dir->cond) == dir->cond)
		{
			switch (dir->type)
			{
			case KD_KEYCODE:
				*keycode = (int) dir->par;
				break;
			case KD_KEYCHAR:
				*keychar = dir->par;
				break;
			case KD_MODIFIER:
				if (evtype == GWM_EVENT_DOWN) kblCurrentMod |= (int) dir->par;
				else kblCurrentMod &= ~((int)dir->par);
				break;
			case KD_TOGGLE:
				if (evtype == GWM_EVENT_DOWN) kblCurrentMod ^= (int) dir->par;
				break;
			};
		};
	};
	
	*keymod = kblCurrentMod;
	pthread_mutex_unlock(&kblLock);
};
