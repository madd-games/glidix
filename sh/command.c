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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "command.h"
#include "strops.h"
#include "dict.h"
#include "sh.h"

#define	WHITESPACES	" \t\n"

static int findExecPath(const char *cmd, char *path)
{
	if (strchr(cmd, '/') != NULL)
	{
		if (strlen(cmd) >= PATH_MAX) return -1;
		else
		{
			strcpy(path, cmd);
			return 0;
		};
	}
	else
	{
		const char *pathspec = dictGet(&dictEnviron, "PATH");
		if (pathspec == NULL)
		{
			return -1;
		};
		
		char *pathlist = strdup(pathspec);
		char *saveptr;
		char *token;
		
		for (token=strtok_r(pathlist, ":", &saveptr); token!=NULL; token=strtok_r(NULL, ":", &saveptr))
		{
			if ((strlen(token) + strlen(cmd) + 1) < PATH_MAX)
			{
				sprintf(path, "%s/%s", token, cmd);
				if (access(path, X_OK) == 0)
				{
					free(pathlist);
					return 0;
				};
			};
		};
		
		free(pathlist);
		return -1;
	};
};

int cmdRun(char *cmd)
{
	Dict localEnviron;
	dictInitFrom(&localEnviron, dictEnviron.list);
	
	int count = 0;
	char **argv = NULL;
	
	char *strp = cmd;
	char *token;
	
	for (token=str_token(&strp, WHITESPACES, "\"'"); token!=NULL; token=str_token(&strp, WHITESPACES, "\"'"))
	{
		int index = count++;
		argv = (char**) realloc(argv, sizeof(char*)*count);
		argv[index] = token;
		str_canon(token);
	};
	
	argv = (char**) realloc(argv, sizeof(char*)*(count+1));
	argv[count] = NULL;
	
	int isVarSetting = 1;
	char **ptr;
	for (ptr=argv; *ptr!=NULL; ptr++)
	{
		if (str_find(*ptr, "=", "\"'") == NULL)
		{
			isVarSetting = 0;
		};
	};

	if (isVarSetting)
	{
		for (ptr=argv; *ptr!=NULL; ptr++)
		{
			dictPut(&dictShellVars, *ptr);
		};
		
		return 0;
	}
	else
	{
		for (ptr=argv; *ptr!=NULL; ptr++)
		{
			if (str_find(*ptr, "=", "\"'") == NULL)
			{
				break;
			};
			
			dictPut(&localEnviron, *ptr);
		};

		if (strcmp(*ptr, "exit") == 0)
		{
			if (ptr[1] == NULL)
			{
				exit(0);
			}
			else
			{
				int exitStatus;
				sscanf(ptr[1], "%d", &exitStatus);
				exit(exitStatus);
			};
		}
		else if (strcmp(*ptr, "export") == 0)
		{
			for (ptr++; *ptr!=NULL; ptr++)
			{
				if (strchr(*ptr, '=') != NULL)
				{
					dictPut(&dictEnviron, *ptr);
				}
				else
				{
					const char *currentValue = dictGet(&dictShellVars, *ptr);
					if (currentValue == NULL)
					{
						currentValue = "";
					};
					
					char spec[strlen(currentValue) + strlen(*ptr) + 2];
					sprintf(spec, "%s=%s", *ptr, currentValue);
					
					dictPut(&dictEnviron, spec);
				};
			};
			
			free(argv);
			return 0;
		}
		else if (strcmp(*ptr, "cd") == 0)
		{
			const char *dir;
			if (ptr[1] == NULL)
			{
				dir = dictGet(&dictEnviron, "HOME");
				if (dir == NULL)
				{
					dir = "/";
				};
			}
			else
			{
				dir = ptr[1];
			};
			
			free(argv);
			if (chdir(dir) != 0)
			{
				fprintf(stderr, "cd: %s: %s\n", dir, strerror(errno));
				return 1;
			};
			
			return 0;
		}
		else if (strcmp(*ptr, ".") == 0)
		{
			if (ptr[1] == NULL)
			{
				fprintf(stderr, "SYNTAX:\t. <script>\n");
				free(argv);
				return 1;
			};
			
			FILE *fp = fopen(ptr[1], "r");
			if (fp == NULL)
			{
				fprintf(stderr, "%s: %s\n", ptr[1], strerror(errno));
				free(argv);
				return 1;
			};
			
			free(argv);
			shSource(fp);
			return 0;
		}
		else if (strcmp(*ptr, "exec") == 0)
		{
			ptr++;
			if (ptr == NULL)
			{
				fprintf(stderr, "USAGE:\texec <command> [args]...\n");
				return 1;
			};
			
			char execPath[PATH_MAX];
			if (findExecPath(*ptr, execPath) != 0)
			{
				fprintf(stderr, "%s: command not found\n", *ptr);
				free(argv);
				return 1;
			};
			
			execve(execPath, ptr, localEnviron.list);
			fprintf(stderr, "exec %s: %s\n", *ptr, strerror(errno));
			return 1;
		}
		else
		{
			char execPath[PATH_MAX];
			if (findExecPath(*ptr, execPath) != 0)
			{
				fprintf(stderr, "%s: command not found\n", *ptr);
				free(argv);
				return 1;
			};
		
			pid_t pid = fork();
			if (pid == -1)
			{
				fprintf(stderr, "fork: %s\n", strerror(errno));
				free(argv);
				return 1;
			}
			else if (pid == 0)
			{
				setpgrp();
				tcsetpgrp(0, getpgrp());
				execve(execPath, ptr, localEnviron.list);
				fprintf(stderr, "exec %s: %s\n", execPath, strerror(errno));
				exit(1);
			}
			else
			{
				free(argv);
				int status;
				while (waitpid(pid, &status, 0) == -1);		// ignore the EINTRs
				tcsetpgrp(0, getpgrp());

				if (WIFSIGNALLED(status))
				{
					const char *colA = "";
					const char *colB = "";
					if (isatty(0))
					{
						colA = "\e\"\x04";
						colB = "\e\"\x07";
					};
					
					int termsig = WTERMSIG(status);
					switch (termsig)
					{
					case SIGSEGV:
						fprintf(stderr, "%sInvalid memory access%s\n", colA, colB);
						break;
					case SIGSYS:
						fprintf(stderr, "%sInvalid system call%s\n", colA, colB);
						break;
					case SIGFPE:
						fprintf(stderr, "%sArithmetic error%s\n", colA, colB);
						break;
					case SIGILL:
						fprintf(stderr, "%sIllegal instruction%s\n", colA, colB);
						break;
					};
				};

				return status;
			};
		};
	};
};
