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
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dirent.h>

#include "lang.h"

typedef struct ContextMap_
{
	struct ContextMap_*		next;
	const char*			name;
	LangRule*			rules;
} ContextMap;

LangRule* getContext(ContextMap *map, const char *name)
{
	ContextMap *scan;
	for (scan=map; scan!=NULL; scan=scan->next)
	{
		if (strcmp(scan->name, name) == 0) return map->rules;
	};
	
	return NULL;
};

int getTokenType(const char *name)
{
	if (strcmp(name, "_blank") == 0)
	{
		return TOK_BLANK;
	}
	else if (strcmp(name, "_keyword") == 0)
	{
		return TOK_KEYWORD;
	}
	else if (strcmp(name, "_type") == 0)
	{
		return TOK_TYPE;
	}
	else if (strcmp(name, "_const") == 0)
	{
		return TOK_CONST;
	}
	else if (strcmp(name, "_number") == 0)
	{
		return TOK_NUMBER;
	}
	else if (strcmp(name, "_builtin") == 0)
	{
		return TOK_BUILTIN;
	}
	else if (strcmp(name, "_comment") == 0)
	{
		return TOK_COMMENT;
	}
	else if (strcmp(name, "_string") == 0)
	{
		return TOK_STRING;
	}
	else if (strcmp(name, "_preproc") == 0)
	{
		return TOK_PREPROC;
	}
	else
	{
		return -1;
	};
};

void loadLangFile(GWMOptionMenu *optLang, const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "Error: failed to read `%s': %s\n", filename, strerror(errno));
		return;
	};
	
	const char *langName = NULL;
	ContextMap *ctxMap = NULL;
	
	char linebuf[2048];
	while (fgets(linebuf, 2048, fp) != NULL)
	{
		char *endLine = strchr(linebuf, '\n');
		if (endLine != NULL) *endLine = 0;
		
		char *line = linebuf;
		while (*line != 0 && strchr(" \t", *line) != NULL) line++;
		
		if (line[0] == '#') continue;
		if (line[0] == 0) continue;
		
		char *cmd = strtok(line, " \t");
		if (cmd == NULL) continue;
		
		if (strcmp(cmd, "name") == 0)
		{
			char *name = strtok(NULL, "");
			if (name == NULL)
			{
				fprintf(stderr, "Error: %s: The `name' directive expects a parameter\n", filename);
				break;
			};
			
			langName = strdup(name);
		}
		else if (strcmp(cmd, "mime") == 0)
		{
			// TODO
		}
		else if (strcmp(cmd, "context") == 0)
		{
			char *ctxName = strtok(NULL, " \t");
			if (ctxName == NULL)
			{
				fprintf(stderr, "Error: %s: The `context' directive expects a parameter\n", filename);
				break;
			};
			
			ctxName = strdup(ctxName);
			char *brace = strtok(NULL, " \t");
			if (brace == NULL || strcmp(brace, "{") != 0)
			{
				fprintf(stderr, "Error: %s: Expected '{' fater `context'\n", filename);
				break;
			};
			
			if (getContext(ctxMap, ctxName) != NULL)
			{
				fprintf(stderr, "Error: %s: redefinition of existing context `%s'\n", filename, ctxName);
				break;
			};
			
			if (ctxName[0] == '_')
			{
				fprintf(stderr, "Error: %s: context name cannot begin with an underscore (we have `%s')\n",
					filename, ctxName);
				break;
			};
			
			LangRule *firstRule = NULL;
			LangRule *lastRule = NULL;
			
			while (fgets(linebuf, 2048, fp) != NULL)
			{
				char *endLine = strchr(linebuf, '\n');
				if (endLine != NULL) *endLine = 0;
		
				char *line = linebuf;
				while (*line != 0 && strchr(" \t", *line) != NULL) line++;
				
				if (line[0] == '#') continue;
				if (line[0] == 0) continue;
				
				char *cmd = strtok(line, " \t");
				if (cmd == NULL) continue;
				
				if (strcmp(cmd, "}") == 0)
				{
					break;
				}
				else if (strcmp(cmd, "match") == 0)
				{
					char *targetName = strtok(NULL, " \t");
					if (targetName == NULL)
					{
						fprintf(stderr, "Error: %s: The `match' directive expects a parameter\n", filename);
						fclose(fp);
						return;
					};
					
					char *expr = strtok(NULL, "");
					if (expr == NULL)
					{
						expr = "";
					};
					
					Regex *regex = lexCompileRegex(expr);
					if (regex == NULL)
					{
						fprintf(stderr, "Error: %s: failed due to invalid regular expression\n", filename);
						fprintf(stderr, "Failing expression: %s\n", expr);
						fclose(fp);
						return;
					};
					
					LangRule *rule = (LangRule*) malloc(sizeof(LangRule));
					memset(rule, 0, sizeof(LangRule));
					
					rule->regex = regex;
					
					if (targetName[0] == '_')
					{
						// targets beginning with underscore specify tokens
						rule->type = getTokenType(targetName);
						if (rule->type == -1)
						{
							fprintf(stderr, "Error: %s: `%s' is not a valid token type\n", filename, targetName);
							fclose(fp);
							return;
						};
					}
					else
					{
						// other targets mean previously-defined contexts
						rule->subctx = getContext(ctxMap, targetName);
						if (rule->subctx == NULL)
						{
							fprintf(stderr, "Error: %s: referring to undefined context `%s'\n",
								filename, targetName);
							fclose(fp);
							return;
						};
					};
					
					if (lastRule == NULL)
					{
						firstRule = lastRule = rule;
					}
					else
					{
						lastRule->next = rule;
						lastRule = rule;
					};
				}
				else
				{
					fprintf(stderr, "Error: %s: Unknown directive `%s' inside context\n", filename, cmd);
					fclose(fp);
					return;
				};
			};
			
			ContextMap *newEl = (ContextMap*) malloc(sizeof(ContextMap));
			newEl->next = ctxMap;
			newEl->name = ctxName;
			newEl->rules = firstRule;
			ctxMap = newEl;
		}
		else
		{
			fprintf(stderr, "Error: %s: Unknown directive `%s'\n", filename, cmd);
			break;
		};
	};

	fclose(fp);

	if (langName == NULL)
	{
		fprintf(stderr, "Error: %s: No language name specified\n", filename);
		return;
	};
	
	LangRule *ctxMain = getContext(ctxMap, "main");
	if (ctxMain == NULL)
	{
		fprintf(stderr, "Error: %s: `main' context not defined\n", filename);
		return;
	};
	
	gwmAddOptionMenu(optLang, (uint64_t) ctxMain, langName);
};

void loadLangs(GWMOptionMenu *optLang)
{
	gwmAddOptionMenu(optLang, 0, "Plain text");
	
	DIR *dirp = opendir("/usr/share/minipad/langs");
	if (dirp == NULL)
	{
		fprintf(stderr, "Error: could not scan langs directory: %s\n", strerror(errno));
		return;
	};
	
	struct dirent *ent;
	while ((ent = readdir(dirp)) != NULL)
	{
		if (ent->d_name[0] == '.') continue;
		if (strlen(ent->d_name) < 5) continue;
		if (strcmp(&ent->d_name[strlen(ent->d_name)-5], ".lang") != 0) continue;
		
		char *filename;
		asprintf(&filename, "/usr/share/minipad/langs/%s", ent->d_name);
		
		fprintf(stderr, "Loading language file `%s'\n", filename);
		loadLangFile(optLang, filename);
		free(filename);
	};
	
	closedir(dirp);
};
