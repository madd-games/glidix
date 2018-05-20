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
	return ctx;
};

void gpmDestroyContext(GPMContext *ctx)
{
	free(ctx->basedir);
	close(ctx->lockfd);
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
			
			sscanf(param, "%hhu.%hhu.%hhu-rc%hhu", &parse.major, &parse.minor, &parse.patch, &parse.rc);
			fclose(fp);
			return parse.value;
		};
	};
	
	fclose(fp);
	return 0;
};
