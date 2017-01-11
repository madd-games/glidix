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

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <locale.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>

#include "strops.h"
#include "dict.h"
#include "preproc.h"
#include "command.h"
#include "sh.h"

#define	MAX_STACK_DEPTH			32

static char username[256] = "unknown";
static char *inlineCommand = NULL;

enum
{
	MODE_INTERACTIVE,
	MODE_INLINE,
	MODE_FILE,
};
static int shellMode = MODE_INTERACTIVE;
static FILE *inputFile = NULL;

typedef struct
{
	int shellMode;
	char *inlineCommand;
	FILE *inputFile;
} StackFrame;

static StackFrame stack[MAX_STACK_DEPTH];
static int stackIndex = 0;

int shLastStatus = 0;
int shScriptArgc;
char **shScriptArgs;

void shSource(FILE *script)
{
	if (stackIndex == MAX_STACK_DEPTH)
	{
		fprintf(stderr, "%sShell stack exhausted!%s\n", "\e\"\x04", "\e\"\x07");
		exit(1);
	};
	
	stack[stackIndex].shellMode = shellMode;
	stack[stackIndex].inlineCommand = inlineCommand;
	stack[stackIndex].inputFile = inputFile;
	stackIndex++;
	
	shellMode = MODE_FILE;
	inlineCommand = NULL;
	inputFile = script;
};

/**
 * Fetch a line of input, and return a pointer to it on the heap. The returned value shall
 * be passed to free() later. If 'cont' is 1, that means we are asking for a line continuation;
 * otherwise we're asking for the beginning of a line (this is only used for interactive input,
 * where the line continuation prompt is shown as ">"). Returns NULL on end of file.
 */
static char *shGetLine(int cont)
{
	if (shellMode == MODE_INLINE)
	{
		if (inlineCommand == NULL) return NULL;
		else
		{
			char *result = strdup(inlineCommand);
			inlineCommand = NULL;
			return result;
		};
	};
	
	if (shellMode == MODE_INTERACTIVE)
	{
		if (cont)
		{
			printf("> ");
		}
		else
		{
			char cwd[PATH_MAX];
			getcwd(cwd, PATH_MAX);
		
			const char *promptColor;
			char prompt;
			if (getuid() == 0)
			{
				prompt = '#';
				promptColor = "\e\"\x04";
			}
			else
			{
				prompt = '$';
				promptColor = "\e\"\x02";
			};
		
			printf("%s%s%s:%s%s%s%c%s ",
				"\e\"\x09", username, "\e\"\x07", "\e\"\x06", cwd, promptColor, prompt, "\e\"\x07");
		};
	};

	char *line = (char*) malloc(1);
	line[0] = 0;
	
	char linebuf[4096];
	while (1)
	{
		if (fgets(linebuf, 4096, inputFile) == NULL)
		{
			if (errno == EINTR)
			{
				free(line);
				printf("\n");
				return strdup("");
			};
			if (line[0] == 0) return NULL;
			break;
		};
		
		char *newline = str_concat(line, linebuf);
		free(line);
		line = newline;
		
		if (strlen(linebuf) != 0)
		{
			if (linebuf[strlen(linebuf)-1] == '\n') break;
		};
	};
	
	line[strlen(line)-1] = 0;	// remove terminating '\n'
	return line;
};

char *shFetch()
{
	char *line = strdup("");
	
	int cont = 0;
	do
	{
		if (line[strlen(line)-1] == '\\')
		{
			line[strlen(line)-1] = 0;
		};
		
		char *nextBit = shGetLine(cont);
		cont = 1;
		
		if (nextBit == NULL)
		{
			if (line[0] == 0)
			{
				free(line);
				return NULL;
			};
			
			break;
		};
		
		char *commentStart = str_find(nextBit, "#", "\"'");
		if (commentStart != NULL)
		{
			*commentStart = 0;
		};
		
		char *newline = str_concat(line, nextBit);
		free(line);
		free(nextBit);
		line = newline;
	} while (line[strlen(line)-1] == '\\');
	
	return line;
};

void on_signal(int sig, siginfo_t *si, void *ignore)
{

};

int main(int argc, char *argv[], char *initEnv[])
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_sigaction = on_signal;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGINT, &sa, NULL) != 0)
	{
		perror("sigaction SIGINT");
		return 1;
	};

	struct passwd *pwd = getpwuid(geteuid());
	if (pwd != NULL) strcpy(username, pwd->pw_name);

	dictInitFrom(&dictEnviron, initEnv);
	dictInit(&dictShellVars);
	
	shScriptArgc = argc - 1;
	shScriptArgs = &argv[1];
	
	int i;
	const char *filename = NULL;
	for (i=1; i<argc; i++)
	{
		if (strcmp(argv[i], "-s") == 0)
		{
			shellMode = MODE_INTERACTIVE;
		}
		else if (strcmp(argv[i], "-c") == 0)
		{
			i++;
			shellMode = MODE_INLINE;
			
			if (argv[i] == NULL)
			{
				fprintf(stderr, "%s: the -c option requires a parameter\n", argv[0]);
				return 1;
			};
			
			inlineCommand = argv[i];
		}
		else if (strcmp(argv[i], "-x") == 0)
		{
			shellMode = MODE_FILE;
		}
		else if (argv[i][0] != '-')
		{
			filename = argv[i];
			shScriptArgc = argc - i;
			shScriptArgs = &argv[i];
			shellMode = MODE_FILE;
			break;
		}
		else
		{
			fprintf(stderr, "%s: unrecognised command-line option: %s\n", argv[0], argv[i]);
			return 1;
		};
	};
	
	if ((shellMode == MODE_FILE) && (filename == NULL))
	{
		fprintf(stderr, "%s: -x passed but no input file\n", argv[0]);
		return 1;
	};
	
	if (shellMode == MODE_FILE)
	{
		inputFile = fopen(filename, "r");
		if (inputFile == NULL)
		{
			fprintf(stderr, "%s: cannot open %s: %s\n", argv[0], filename, strerror(errno));
			return 1;
		};
	}
	else
	{
		inputFile = stdin;
	};
	
	while (1)
	{
		while (1)
		{
			char *line = shFetch();
			if (line == NULL) break;
			line = preprocLine(line);
			shLastStatus = cmdRun(line);
			free(line);
		};
		
		if (stackIndex == 0)
		{
			break;
		};
		
		if (inputFile != NULL)
		{
			if (inputFile != stdin)
			{
				fclose(inputFile);
			};
		};
		
		stackIndex--;
		shellMode = stack[stackIndex].shellMode;
		inlineCommand = stack[stackIndex].inlineCommand;
		inputFile = stack[stackIndex].inputFile;
	};
	
	return 0;
};
