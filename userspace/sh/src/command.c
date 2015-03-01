/*
	Glidix Shell

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

#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "command.h"

int findCommand(char *path, char *cmd)
{
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

int execCommand(char *cmd)
{
	int argc = 0;
	char **argv = NULL;
	char *nextToStrtok = cmd;

	while (1)
	{
		char *token = strtok(nextToStrtok, " \t");
		nextToStrtok = NULL;
		if (token != NULL)
		{
			if (strlen(token) == 0) continue;
		};

		argv = realloc(argv, sizeof(char*)*(argc+1));
		argv[argc++] = token;

		if (token == NULL) break;
	};

	if (strcmp(argv[0], "cd") == 0)
	{
		int status = cmd_cd(argc-1, argv);
		free(argv);
		return status;
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
		if (execv(execpath, argv) != 0)
		{
			perror(argv[0]);
			return 1;
		};

		// the compiler won't stop moaning otherwise.
		// even though execv() obviously doesn't return.
		return 0;
	}
	else
	{
		free(argv);
		int status;
		waitpid(pid, &status, 0);
		return status;
	};
};
