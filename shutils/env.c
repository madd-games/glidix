/*
	Glidix Shell Utilities

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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int findCommand(char *path, const char *cmd)
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

char path[256];

int main(int argc, char *argv[], char *envp[])
{
	if (argc < 2)
	{
		char **scan;
		for (scan=envp; *scan!=NULL; scan++)
		{
			printf("%s\n", *scan);
		};

		return 0;
	};

	if (findCommand(path, argv[1]) != 0)
	{
		fprintf(stderr, "%s: %s: command not found\n", argv[0], argv[1]);
		return 1;
	};

	if (execv(path, &argv[1]) != 0)
	{
		perror(path);
		return 1;
	};

	// never reached
	return 0;
};
