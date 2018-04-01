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
#include <termios.h>

#include "strops.h"
#include "dict.h"
#include "preproc.h"
#include "command.h"
#include "sh.h"
#include "colorize.h"

#define	MAX_STACK_DEPTH			32
#define	MAX_BRACKET_DEPTH		24

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

typedef struct CmdHistory_ 
{
	char *line;
	struct CmdHistory_ *next;
	struct CmdHistory_ *prev;
} CmdHistory;

struct CmdHistory_ *cmdHead;
		
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

void shInline(char *line)
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
	
	shellMode = MODE_INLINE;
	inlineCommand = line;
	inputFile = NULL;
};

void shClearStack()
{
	stackIndex = 0;
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
			char *incmd = inlineCommand;
			char *linebreak = strchr(inlineCommand, '\n');
			if (linebreak != NULL)
			{
				*linebreak = 0;
				inlineCommand = linebreak + 1;
			}
			else
			{
				inlineCommand = NULL;
			};
			
			return strdup(incmd);
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
		
		struct termios tc;
		tcgetattr(0, &tc);
		struct termios old_tc = tc;
		tc.c_lflag &= ~(ECHO);
		tc.c_lflag &= ~ICANON;
		tcsetattr(0, TCSANOW, &tc);
		
		char linbuff [1024];
		char pendbuff [1024];
		char c;
		size_t numChars = 0;
		CmdHistory *currCmd = NULL;
		
		while (1)
		{	
			ssize_t retval = read(0, &c, 1);
			
			if(retval == -1)
			{
				if (errno == EINTR)
				{
					printf("\n");
					return strdup("");
				}
				else return NULL;
			}
			
			if(retval == 0) return NULL;
			else
			{
				if(c == '\n')
				{
					if(numChars != 0)
					{
						CmdHistory *cmd = (CmdHistory*) malloc(sizeof(CmdHistory));
						cmd->prev = cmdHead;
						cmd->next = NULL;
						if (cmdHead != NULL) cmdHead->next = cmd;
						cmdHead = cmd;
						linbuff[numChars] = 0;
						cmd->line = strdup(linbuff);
					}
					linbuff[numChars] = 0;
					tcsetattr(0, TCSANOW, &old_tc);
					return strdup(linbuff);
				}
				else if((uint8_t)c == 0x8B)
				{
					if((currCmd == NULL) && (cmdHead != NULL))
					{
						currCmd = cmdHead;
						linbuff[numChars] = 0;
						strcpy(pendbuff, linbuff);
						while (numChars--) printf("\b");
						numChars = strlen(currCmd->line);
						strcpy(linbuff, currCmd->line);
						//printf("%s", linbuff);
						printColored(linbuff, strlen(linbuff));
					}
					else if(currCmd != NULL)
					{
						if(currCmd->prev != NULL)
						{
							currCmd = currCmd->prev;
							while (numChars--) printf("\b");
							numChars = strlen(currCmd->line);
							strcpy(linbuff, currCmd->line);
							//printf("%s", linbuff);
							printColored(linbuff, strlen(linbuff));
						}
					}
				}
				else if((uint8_t)c == 0x8C)
				{
					if(currCmd != NULL)
					{
						currCmd = currCmd->next;
						while (numChars--) printf("\b");
						if(currCmd == NULL)
						{
							strcpy(linbuff, pendbuff);
							numChars = strlen(linbuff);
							//printf("%s", linbuff);
							printColored(linbuff, strlen(linbuff));
						}
						else
						{
							numChars = strlen(currCmd->line);
							strcpy(linbuff, currCmd->line);
							//printf("%s", linbuff);
							printColored(linbuff, strlen(linbuff));
						}
					}
				}
				else if(c == '\b')
				{
					if (numChars != 0)
					{
						int count = numChars;
						while (count--) printf("\b");
					
						numChars--;
						printColored(linbuff, numChars);
					};
				}
				else if(numChars < 1023) 
				{
					int count = numChars;
					while (count--) printf("\b");
					
					linbuff[numChars++] = c;
					printColored(linbuff, numChars);
				} 
			}
		}
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

/**
 * Check if string is "complete" (all brackets closed). Return values:
 * 0 - all brackets closed and coherent
 * 1 - a close-bracket symbol without matching open-bracket was spotted
 * 2 - maximum bracket depth exceeded
 * other - the bottom-level close-bracket character we are waiting for (unclosed brackets).
 */
char shGetExpected(const char *str)
{
	// each element on the stack is the close-bracket symbol necessary
	char bracketStack[MAX_BRACKET_DEPTH];
	int bracketDepth = 0;
	
	const char *brOpens = 	"\"'({";
	const char *brCloses =  "\"')}";
	const char *nonNestingBrackets = "\"'";
	
	for (; *str!=0; str++)
	{
		if (*str == '\\')
		{
			str++;
			continue;
		};
		
		if (bracketDepth != 0)
		{
			if (bracketStack[bracketDepth-1] == *str)
			{
				bracketDepth--;
				continue;
			};
			
			// do not check for bad bracket closings or new open brackets inside non-nesting brackets
			if (strchr(nonNestingBrackets, bracketStack[bracketDepth-1]) != NULL)
			{
				continue;
			};
		};
		
		// check for open-brackets
		char *matchChar = strchr(brOpens, *str);
		if (matchChar != NULL)
		{
			if (bracketDepth == MAX_BRACKET_DEPTH)
			{
				// maximum depth exceeded
				return 2;
			};
			
			const char *closer = brCloses + (matchChar - brOpens);
			bracketStack[bracketDepth++] = *closer;
			continue;
		};

		// any close-bracket here is a mismatch
		if (strchr(brCloses, *str) != NULL)
		{
			return 1;
		};
	};
	
	if (bracketDepth == 0) return 0;
	else return bracketStack[bracketDepth-1];
};

char *shFetch()
{
	char *line = strdup("");
	
	int cont = 0;
	while (1)
	{
		char *nextBit = shGetLine(cont);
		if (nextBit == NULL)
		{
			if (line[0] == 0)
			{
				free(line);
				return NULL;
			};
			
			break;
		};
		
		cont = 1;
		
		char *commentStart = str_find(nextBit, "#", "\"'");
		if (commentStart != NULL)
		{
			*commentStart = 0;
		};
		
		// remove whitespaces from the end
		while (strlen(nextBit) != 0)
		{
			char *scan = &nextBit[strlen(nextBit)-1];
			if (strchr(" \t\n", *scan) != NULL) *scan = 0;
			else break;
		};
		
		// line continuation if ends in '\'
		if (strlen(nextBit) != 0)
		{
			if (nextBit[strlen(nextBit)-1] == '\\')
			{
				nextBit[strlen(nextBit)-1] = 0;
				char *newline = str_concat(line, nextBit);
				free(line);
				free(nextBit);
				line = newline;
				continue;
			};
		};

		char *newline = str_concat(line, nextBit);
		free(line);
		free(nextBit);
		line = newline;

		// line continuation if incomplete
		if (shGetExpected(line) >= 10)
		{
			newline = str_concat(line, "\n");
			free(line);
			line = newline;
			continue;
		};
		
		// in all other cases break
		break;
	};
	
	// check if good
	char status = shGetExpected(line);
	if (status == 0)
	{
		return line;
	}
	else if (status == 1)
	{
		fprintf(stderr, "sh: close-bracket without a matching open-bracket\n");
		free(line);
		if (shellMode == MODE_INTERACTIVE) return strdup("");
		return NULL;
	}
	else if (status == 2)
	{
		fprintf(stderr, "sh: bracket depth limit exceeded\n");
		free(line);
		if (shellMode == MODE_INTERACTIVE) return strdup("");
		return NULL;
	}
	else
	{
		fprintf(stderr, "sh: expected `%c' before EOF\n", status);
		free(line);
		return NULL;
	};
};

void on_signal(int sig, siginfo_t *si, void *ignore)
{

};

void shPop()
{
	stackIndex--;
	shellMode = stack[stackIndex].shellMode;
	inlineCommand = stack[stackIndex].inlineCommand;
	inputFile = stack[stackIndex].inputFile;
};

int shRun()
{
	shLastStatus = 0;
	int bottomFrame = stackIndex;
	
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
		
		if (stackIndex == bottomFrame)
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
		
		shPop();
	};
	
	return shLastStatus;
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
			
			inlineCommand = strdup(argv[i]);
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

	return shRun();
};
