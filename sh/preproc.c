/*
	Glidix Shell

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

#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "strops.h"
#include "dict.h"
#include "sh.h"

const char* getVarValue(const char *varname)
{
	const char *value = dictGet(&dictShellVars, varname);
	if (value == NULL)
	{
		value = dictGet(&dictEnviron, varname);
	};
	
	return value;
};

static char* preprocVars(char *line)
{
	static const char *nameChars = "_0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char *result = strdup("");
	
	char *scan = line;
	
	while (*scan != 0)
	{
		char *nextDollar = str_find(scan, "$", "'");
		if (nextDollar == NULL)
		{
			char *newline = str_concat(result, scan);
			free(result);
			result = newline;
			break;
		};
		
		char *newline = str_concatn(result, scan, nextDollar);
		free(result);
		result = newline;
		
		scan = nextDollar;
		scan++;
		
		char varname[256];
		const char *value = NULL;
		char numbuf[64];
		int argIndex;
		
		if (*scan == '?')
		{
			scan++;
			sprintf(numbuf, "%d", shLastStatus);
			value = numbuf;
		}
		else if (*scan == '$')
		{
			scan++;
			sprintf(numbuf, "%d", getpid());
			value = numbuf;
		}
		else if (sscanf(scan, "%d", &argIndex) == 1)
		{
			while ((*scan >= '0') && (*scan <= '9')) scan++;
			if (argIndex >= shScriptArgc)
			{
				value = NULL;
			}
			else
			{
				value = shScriptArgs[argIndex];
			};
		}
		else if (*scan == '#')
		{
			scan++;
			sprintf(numbuf, "%d", shScriptArgc);
			value = numbuf;
		}
		else
		{
			char *put = varname;
			while ((strchr(nameChars, *scan) != NULL) && (*scan != 0))
			{
				if ((put-varname) > 255)
				{
					// name too long
					break;
				};
			
				*put++ = *scan++;
			};
		
			*put = 0;
		
			value = getVarValue(varname);
		};
		
		if (value != NULL)
		{
			char *newline = str_concat(result, value);
			free(result);
			result = newline;
		};
	};
	
	free(line);
	return result;
};

static char* preprocBackticks(char *line)
{
	char *result = strdup("");
	char *scan = line;
	
	while (*scan != 0)
	{
		char *nextBacktick = str_find(scan, "`", "'");
		if (nextBacktick == NULL)
		{
			char *newline = str_concat(result, scan);
			free(result);
			result = newline;
			break;
		};
		
		char *newline = str_concatn(result, scan, nextBacktick);
		free(result);
		result = newline;
		
		scan = nextBacktick + 1;
		nextBacktick = str_find(scan, "`", "'");
		if (nextBacktick == NULL)
		{
			break;
		};
		
		*nextBacktick = 0;
		char *subcommand = scan;
		scan = nextBacktick + 1;
		
		int pipefd[2];
		if (pipe(pipefd) != 0)
		{
			continue;
		};
		
		pid_t pid = fork();
		if (pid == -1)
		{
			continue;
		}
		else if (pid == 0)
		{
			close(pipefd[0]);
			close(1);
			dup(pipefd[1]);
			close(pipefd[1]);
			execl("/proc/self/exe", "sh", "-c", subcommand, NULL);
			exit(1);
		}
		else
		{
			close(pipefd[1]);
			
			char buffer[256];
			while (1)
			{
				ssize_t sz = read(pipefd[0], buffer, 255);
				if (sz == -1) break;
				if (sz == 0) break;
				buffer[sz] = 0;
				
				char *edit;
				for (edit=buffer; *edit!=0; edit++)
				{
					if (*edit == '\n') *edit = ' ';
				};
				
				char *newline = str_concat(result, buffer);
				free(result);
				result = newline;
			};
			
			close(pipefd[0]);
			waitpid(pid, NULL, 0);
		};
	};
	
	free(line);
	return result;
};

char *preprocAutospace(char *line)
{
	// insert automatic spaces in things like "echo>test" -> "echo > test"
	char *out = (char*) malloc(strlen(line) * 3 + 1);
	char *put = out;
	char *scan = line;
	
	char quote = 0;
	while (*scan != 0)
	{
		if (quote != 0)
		{
			*put++ = *scan;
			if (quote == *scan) quote = 0;
			scan++;
		}
		else if (*scan == '>')
		{
			if (scan != line)
			{
				if ((scan[-1] < '0') || (scan[-1] > '9'))
				{
					*put++ = ' ';
				};
			};
			
			*put++ = *scan++;
			*put++ = ' ';
		}
		else
		{
			char c = *scan++;
			*put++ = c;
			
			if ((c == '\'') || (c == '"'))
			{
				quote = c;
			};
		};
	};
	
	*put = 0;
	return out;
};

char *preprocLine(char *line)
{
	line = preprocVars(line);
	line = preprocBackticks(line);
	line = preprocAutospace(line);
	return line;
};
