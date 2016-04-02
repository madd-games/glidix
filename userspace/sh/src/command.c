/*
	Glidix Shell

	Copyright (c) 2014-2016, Madd Games.
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

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/glidix.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include "command.h"
#include "sh.h"

int findCommand(char *path, char *cmd)
{
	const char *scan = cmd;
	while (*scan != 0)
	{
		if (*scan == '/')
		{
			strcpy(path, cmd);
			return 0;
		};
		scan++;
	};

	char *search = strdup(getenv("PATH"));
	char *token = strtok(search, ":");

	do
	{
		strcpy(path, token);
		if (path[strlen(path)-1] != '/') strcat(path, "/");
		strcat(path, cmd);

		struct stat st;
		if (stat(path, &st) == 0)
		{
			free(search);
			return 0;
		};
		token = strtok(NULL, ":");
	} while (token != NULL);

	free(search);
	return -1;
};

int cmd_cd(int argc, char **argv);
int cmd_exit(int argc, char **argv);
int cmd_echo(int argc, char **argv);

int execCommand(char *cmd)
{
	if (*cmd == '#') return 0;

	// remove comments
	int currentlyInString = 0;
	char *commentScan = cmd;

	while (*commentScan != 0)
	{
		if ((*commentScan) == '"') currentlyInString = !currentlyInString;
		else if (((*commentScan) == '#') && (!currentlyInString))
		{
			*commentScan = 0;
			break;
		};

		commentScan++;
	};

	// stdout redirection
	const char *outfile = NULL;
	char *arrowPtr = strrchr(cmd, '>');
	if (arrowPtr != NULL)
	{
		*arrowPtr = 0;
		outfile = arrowPtr + 1;
	};
	
	if (*cmd == 0) return 0;

	char processedCommand[1024];
	char *put = processedCommand;
	char *scan = cmd;
	while (*scan != 0)
	{
		if (*scan == '\\')
		{
			scan++;
			if (*scan == 0) break;
			*put++ = *scan++;
			continue;
		}
		else if (*scan == '$')
		{
			char envname[256];
			char *envnameput = envname;
			scan++;
			while (isalnum(*scan))
			{
				*envnameput++ = *scan++;
			};

			*envnameput = 0;
			char *value = getenv(envname);
			if (value != NULL)
			{
				strcpy(put, value);
				put += strlen(value);
			};
		}
		else
		{
			*put++ = *scan++;
		};
	};
	*put = 0;

	int argc = 0;
	char **argv = NULL;
	char *nextToStrtok = processedCommand;

	while (1)
	{
		char *token = nextToStrtok;

		if (token != NULL)
		{
			while (isspace(*token)) token++;
			if (*token == 0)
			{
				token = NULL;
			}
			else
			{
				const char *termString = " \t";
				if (*token == '"')
				{
					termString = "\"";
					token++;
				};

				char *endpos = strpbrk(token, termString);
				if (endpos == NULL)
				{
					nextToStrtok = NULL;
				}
				else
				{
					*endpos = 0;
					nextToStrtok = endpos+1;
				};
			};
		};

		int shouldAdd = 0;
		if (token == NULL)
		{
			shouldAdd = 1;
		}
		else
		{
			if (strlen(token) > 0)
			{
				shouldAdd = 1;
			};
		};

		if (shouldAdd)
		{
			argv = realloc(argv, sizeof(char*)*(argc+1));
			argv[argc++] = token;
		};

		if (token == NULL) break;
	};

	if (argc == 1) return 0;

	if (strcmp(argv[0], "cd") == 0)
	{
		int status = cmd_cd(argc-1, argv);
		free(argv);
		return status;
	}
	else if (strcmp(argv[0], "exit") == 0)
	{
		int status = cmd_exit(argc-1, argv);
		free(argv);
		return status;
	}
	else if (strcmp(argv[0], "echo") == 0)
	{
		int status = cmd_echo(argc-1, argv);
		free(argv);
		return status;
	}
	else if (strcmp(argv[0], "diag") == 0)
	{
		_glidix_diag();
		free(argv);
		return 0;
	};

	char execpath[256];
	if (findCommand(execpath, argv[0]) == -1)
	{
		free(argv);
		fprintf(stderr, "%s: command not found\n", argv[0]);
		return 1;
	};

	pid_t pid = fork();
	if (pid == 0)
	{
		if (outfile != NULL)
		{
			int fd = open(outfile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
			if (fd == -1)
			{
				fprintf(stderr, "%s: cannot open output file %s: %s\n", argv[0], outfile, strerror(errno));
				exit(1);
			};
			
			close(1);
			dup2(fd, 1);
		};
			
		if (execv(execpath, argv) != 0)
		{
			perror(argv[0]);
			exit(1);
		};

		// the compiler won't stop moaning otherwise.
		// even though execv() obviously doesn't return.
		return 0;
	}
	else
	{
		free(argv);
		shellChildPid = pid;
		int status;
		int ret;
		while (1)
		{
			ret = waitpid(pid, &status, 0);
			
			// EINTR is the only possible error here, so if it occurs, try again.
			if (ret != -1)
			{
				break;
			};
		};

		shellChildPid = 0;
		
		if (WIFSIGNALLED(status))
		{
			int termsig = WTERMSIG(status);
			switch (termsig)
			{
			case SIGSEGV:
				fprintf(stderr, "Invalid memory access\n");
				break;
			case SIGSYS:
				fprintf(stderr, "Invalid system call\n");
				break;
			case SIGFPE:
				fprintf(stderr, "Arithmetic error\n");
				break;
			};
		};
		
		return status;
	};
};
