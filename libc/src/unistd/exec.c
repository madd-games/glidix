/*
	Glidix Runtime

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

#include <unistd.h>
#include <sys/glidix.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>

extern char **environ;

int execve(const char *pathname, char* const argv[], char *const envp[])
{
	size_t argcount = 0;
	size_t argsize = 0;
	char *const *scan = argv;
	while (*scan != NULL)
	{
		argcount++;
		argsize += strlen(*scan);
		scan++;
	};

	size_t envcount = 0;
	size_t envsize = 0;
	scan = envp;
	while (*scan != NULL)
	{
		envcount++;
		envsize += strlen(*scan);
		scan++;
	};

	size_t execparsz = argcount + argsize + envcount + envsize + 4;
	char *execpars = (char*) __builtin_alloca(execparsz);
	memset(execpars, 0, execparsz);

	size_t i;
	size_t offset = 0;
	for (i=0; i<argcount; i++)
	{
		strcpy(&execpars[offset], argv[i]);
		offset += strlen(argv[i]) + 1;
	};
	offset++;

	for (i=0; i<envcount; i++)
	{
		strcpy(&execpars[offset], envp[i]);
		offset += strlen(envp[i]) + 1;
	};

	// obviously _glidix_exec() will only return on failure.
	int ret = _glidix_exec(pathname, execpars, execparsz);
	return ret;
};

int execv(const char *pathname, char *const argv[])
{
	return execve(pathname, argv, environ);
};

static int __execl(const char *pathname, const char *arg0, va_list ap)
{
	char **argv = (char**) malloc(sizeof(char*));
	argv[0] = (char*) arg0;
	int argc = 1;
	
	while (1)
	{
		char *str = va_arg(ap, char*);
		argv = realloc(argv, sizeof(char*)*(argc+1));
		argv[argc++] = str;
		if (str == NULL) break;
	};

	int r = execv(pathname, argv);
	free(argv);
	return r;
};

int execl(const char *pathname, const char *arg0, ...)
{
	va_list ap;
	va_start(ap, arg0);
	int r = __execl(pathname, arg0, ap);
	va_end(ap);
	return r;
};

int execle(const char *pathname, const char *arg0, ...)
{
	char **argv = (char**) malloc(sizeof(char*));
	argv[0] = (char*) arg0;
	int argc = 1;
	va_list ap;
	va_start(ap, arg0);

	while (1)
	{
		char *str = va_arg(ap, char*);
		argv = realloc(argv, sizeof(char*)*(argc+1));
		argv[argc++] = str;
		if (str == NULL) break;
	};

	char **env = va_arg(ap, char**);

	va_end(ap);
	int r = execve(pathname, argv, env);
	free(argv);
	return r;
};

int __find_command(char *path, char *cmd)
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

	char *save;
	char *search = strdup(getenv("PATH"));
	char *token = strtok_r(search, ":", &save);

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
		token = strtok_r(NULL, ":", &save);
	} while (token != NULL);

	free(search);
	return -1;
};

int execvp(const char *cmd, char *const argv[])
{
	if (strchr(cmd, '/') != NULL)
	{
		return execv(cmd, argv);
	};
	
	char path[256];
	char *cmddup = strdup(cmd);
	int ok = __find_command(path, cmddup);
	free(cmddup);
	
	if (ok == -1)
	{
		errno = ENOENT;
		return -1;
	};
	
	return execv(path, argv);
};

int execlp(const char *cmd, const char *arg0, ...)
{
	int r;
	va_list ap;
	va_start(ap, arg0);
	
	if (strchr(cmd, '/') != NULL)
	{
		r = __execl(cmd, arg0, ap);
		va_end(ap);
		return r;
	};
	
	char path[256];
	char *cmddup = strdup(cmd);
	int ok = __find_command(path, cmddup);
	free(cmddup);
	
	if (ok == -1)
	{
		errno = ENOENT;
		return -1;
	};
	
	r = __execl(path, arg0, ap);
	va_end(ap);
	return r;
};
