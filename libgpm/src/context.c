/*
	Glidix Package Manager

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
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/log.h>
#include <sys/wait.h>

#include "libgpm.h"

GPMContext* gpmCreateContext(const char *dest, int *error)
{
	GPMContext *ctx = (GPMContext*) malloc(sizeof(GPMContext));
	if (ctx == NULL)
	{
		if (error != NULL) *error = GPM_ERR_ALLOC;
		return NULL;
	};
	
	if (dest == NULL) dest = "";

	if (asprintf(&ctx->tempdir, "%s/run/gpm-temp-%d", dest) == -1)
	{
		free(ctx);
		if (error != NULL) *error = GPM_ERR_ALLOC;
		return NULL;
	};
	
	if (asprintf(&ctx->basedir, "%s/etc/gpm", dest) == -1)
	{
		free(ctx);
		if (error != NULL) *error = GPM_ERR_ALLOC;
		return NULL;
	};
	
	char *lockpath;
	if (asprintf(&lockpath, "%s/.lock", ctx->basedir) == -1)
	{
		free(ctx->basedir);
		free(ctx);
		if (error != NULL) *error = GPM_ERR_ALLOC;
		return NULL;
	};
	
	int fd = open(lockpath, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if (fd == -1)
	{
		free(ctx->basedir);
		free(ctx);
		free(lockpath);
		if (error != NULL) *error = GPM_ERR_LOCK;
		return NULL;
	};
	
	free(lockpath);
	
	if (lockf(fd, F_TLOCK, 0) != 0)
	{
		free(ctx->basedir);
		free(ctx);
		close(fd);
		if (error != NULL) *error = GPM_ERR_LOCK;
		return NULL;
	};
	
	ctx->dest = strdup(dest);
	ctx->lockfd = fd;
	
	char *logpath;
	if (asprintf(&logpath, "%s/var/log/gpm.log", dest) == -1)
	{
		close(fd);
		free(ctx->basedir);
		free(ctx);
		if (error != NULL) *error = GPM_ERR_ALLOC;
	};
	
	int logfd = __log_open(logpath);
	if (logfd == -1)
	{
		free(logpath);
		close(fd);
		free(ctx->basedir);
		free(ctx);
		if (error != NULL) *error = GPM_ERR_LOCK;
		return NULL;
	};
	
	ctx->log = fdopen(logfd, "w");
	if (ctx->log == NULL)
	{
		close(logfd);
		close(fd);
		free(ctx->basedir);
		free(ctx);
		if (error != NULL) *error = GPM_ERR_LOCK;
		return NULL;
	};
	
	mkdir(ctx->tempdir, 0700);
	return ctx;
};

void gpmDestroyContext(GPMContext *ctx)
{
	// delete the temporary directory
	fflush(ctx->log);
	pid_t pid = fork();
	if (pid == 0)
	{
		close(1);
		close(2);
		dup(fileno(ctx->log));
		dup(1);
		execl("/usr/bin/rm", "rm", "-r", ctx->tempdir, NULL);
		_exit(1);
	}
	else if (pid != -1)
	{
		waitpid(pid, NULL, 0);
	};
	
	free(ctx->basedir);
	close(ctx->lockfd);
	fclose(ctx->log);
	free(ctx);
};

uint32_t gpmGetPackageVersion(GPMContext *ctx, const char *name)
{
	char *descpath;
	if (asprintf(&descpath, "%s/%s.pkg", ctx->basedir, name) == -1)
	{
		return 0;
	};
	
	FILE *fp = fopen(descpath, "r");
	free(descpath);
	
	if (fp == NULL)
	{
		return 0;
	};
	
	char linebuf[1024];
	while (fgets(linebuf, 1024, fp) != NULL)
	{
		char *newline = strchr(linebuf, '\n');
		if (newline != NULL) *newline = 0;
		
		char *space = strchr(linebuf, ' ');
		if (space == NULL) continue;
		
		char *cmd = linebuf;
		*space = 0;
		char *param = space+1;
		
		if (strcmp(cmd, "version") == 0)
		{
			fclose(fp);
			return gpmParseVersion(param);
		};
	};
	
	fclose(fp);
	return 0;
};

uint32_t gpmParseVersion(const char *str)
{
	union
	{
		uint32_t value;
		struct
		{
			uint8_t rc;
			uint8_t patch;
			uint8_t minor;
			uint8_t major;
		};
	} parse;
	
	parse.rc = 0xFF;
	parse.patch = 0;
	parse.minor = 0;
	parse.major = 1;
	
	sscanf(str, "%hhu.%hhu.%hhu-rc%hhu", &parse.major, &parse.minor, &parse.patch, &parse.rc);
	return parse.value;
};

uint32_t gpmGetLatestVersion(GPMContext *ctx, const char *name)
{
	char *indexpath;
	if (asprintf(&indexpath, "%s/repos.index", ctx->basedir) == -1)
	{
		return 0;
	};
	
	FILE *fp = fopen(indexpath, "r");
	free(indexpath);
	
	if (fp == NULL)
	{
		return 0;
	};
	
	uint32_t latest = 0;
	
	char linebuf[1024];
	while (fgets(linebuf, 1024, fp) != NULL)
	{
		char *newline = strchr(linebuf, '\n');
		if (newline != NULL) *newline = 0;
		
		char *comment = strchr(linebuf, '#');
		if (comment != NULL) *comment = 0;
		
		char *saveptr;
		char *pkgname = strtok_r(linebuf, " \t", &saveptr);
		char *verspec = strtok_r(NULL, " \t", &saveptr);
		
		if (pkgname == NULL || verspec == NULL) continue;
		
		if (strcmp(pkgname, name) == 0)
		{
			uint32_t thisVer = gpmParseVersion(verspec);
			if (thisVer > latest) latest = thisVer;
		};
	};
	
	fclose(fp);
	return latest;
};

char* gpmGetPackageURL(GPMContext *ctx, const char *name, uint32_t version)
{
	char *indexpath;
	if (asprintf(&indexpath, "%s/repos.index", ctx->basedir) == -1)
	{
		return NULL;
	};
	
	FILE *fp = fopen(indexpath, "r");
	free(indexpath);
	
	if (fp == NULL)
	{
		return NULL;
	};
	
	char linebuf[1024];
	while (fgets(linebuf, 1024, fp) != NULL)
	{
		char *newline = strchr(linebuf, '\n');
		if (newline != NULL) *newline = 0;
		
		char *comment = strchr(linebuf, '#');
		if (comment != NULL) *comment = 0;
		
		char *saveptr;
		char *pkgname = strtok_r(linebuf, " \t", &saveptr);
		char *verspec = strtok_r(NULL, " \t", &saveptr);
		char *url = strtok_r(NULL, " \t", &saveptr);
		
		if (pkgname == NULL || verspec == NULL || url == NULL) continue;
		
		if (strcmp(pkgname, name) == 0)
		{
			uint32_t thisVer = gpmParseVersion(verspec);
			if (thisVer == version)
			{
				return strdup(url);
			};
		};
	};
	
	fclose(fp);
	return NULL;
};
