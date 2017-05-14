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

#include <stdlib.h>
#include <string.h>
#include <sys/glidix.h>
#include <errno.h>
#include <stdio.h>

extern char **environ;
char __getenv_buf[1024];

char *getenv(const char *name)
{
	const char *namecheck = name;
	while (*namecheck != 0)
	{
		if (*namecheck++ == '=') return NULL;
	};

	size_t namelen = strlen(name);
	char **scan = environ;
	while (*scan != NULL)
	{
		if (strlen(*scan) > namelen)
		{
			if (memcmp(*scan, name, namelen) == 0)
			{
				if ((*scan)[namelen] == '=')
				{
					strcpy(__getenv_buf, &((*scan)[namelen+1]));
					return __getenv_buf;
				};
			};
		};

		scan++;
	};

	return NULL;
};

static int __putenv(char *newent, int update);

int setenv(const char *name, const char *value, int update)
{
	if (name == NULL)
	{
		errno = EINVAL;
		return -1;
	};

	const char *namecheck = name;
	while (*namecheck != 0)
	{
		if (*namecheck++ == '=')
		{
			errno = EINVAL;
			return -1;
		};
	};

	char *newent = malloc(strlen(name) + strlen(value) + 2);
	strcpy(newent, name);
	strcat(newent, "=");
	strcat(newent, value);

	return __putenv(newent, update);
};

static int __putenv(char *newent, int update)
{
	if (strchr(newent, '=') == NULL)
	{
		errno = EINVAL;
		return -1;
	};
	
	size_t namelen = strchr(newent, '=') - newent;
	char **scan = environ;
	int i = 0;
	while (*scan != NULL)
	{
		if (strlen(*scan) > namelen)
		{
			if (memcmp(*scan, newent, namelen) == 0)
			{
				if ((*scan)[namelen] == '=')
				{
					if (update)
					{
						free(*scan);
						*scan = newent;
					}
					else
					{
						free(newent);
					};

					return 0;
				};
			};
		};

		i++;
		scan++;
	};

	environ = realloc(environ, sizeof(char*)*(i+2));
	environ[i+1] = NULL;
	environ[i] = newent;

	return 0;
};

int putenv(const char *ent)
{
	return __putenv(strdup(ent), 1);
};

static void __rmenv(int rmidx)
{
	int i;
	for (i=0; environ[i]!=NULL; i++);
	char **newEnv = (char**) malloc(sizeof(char*)*i);
	memcpy(newEnv, environ, sizeof(char*)*rmidx);
	memcpy(&newEnv[rmidx], &environ[rmidx+1], i-rmidx-1);
	newEnv[i-1] = NULL;
	
	char **oldenv = environ;
	environ = newEnv;
	free(oldenv);
};

int unsetenv(const char *name)
{
	if (name == NULL)
	{
		errno = EINVAL;
		return -1;
	};
	
	if (strchr(name, '=') != NULL)
	{
		errno = EINVAL;
		return -1;
	};
	
	if (*name == 0)
	{
		errno = EINVAL;
		return -1;
	};
	
	int i;
	for (i=0; environ[i]!=NULL; i++)
	{
		if (strlen(environ[i]) > strlen(name))
		{
			if (memcmp(environ[i], name, strlen(name)) == 0)
			{
				if (environ[i][strlen(name)] == '=')
				{
					__rmenv(i);
					return 0;
				};
			};
		};
	};
	
	return 0;
};
