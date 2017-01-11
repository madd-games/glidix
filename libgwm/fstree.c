/*
	Glidix GUI

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
#include <sys/glidix.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <libgwm.h>
#include <errno.h>
#include <dirent.h>

/**
 * A GWMTreeEnum which enumerates the filesystem.
 */

typedef struct
{
	char				path[PATH_MAX];
	DIR*				dirp;
} FSNode;

static const char *fsCols[] = {
	"Name",
	"Type"
};

char gwmFSRoot[PATH_MAX] = "/";

static char* fsGetColCaption(int index)
{
	return strdup(fsCols[index]);
};

static void* fsOpenNode(const void *path)
{
	FSNode *node = (FSNode*) malloc(sizeof(FSNode));
	if (node == NULL) return NULL;
	
	strcpy(node->path, (const char*) path);
	node->dirp = opendir((const char*) path);
	if (node->dirp == NULL)
	{
		free(node);
		return NULL;
	};
	
	return node;
};

static int fsGetNext(void *context, GWMNodeInfo *info, size_t infoSize)
{
	FSNode *node = (FSNode*) context;
	struct dirent *ent;
	
	do
	{
		ent = readdir(node->dirp);
		if (ent == NULL) return -1;
	} while (ent->d_name[0] == '.');
	
	sprintf((char*)info->niPath, "%s/%s", node->path, ent->d_name);
	info->niFlags = 0;
	info->niValues[0] = strdup(ent->d_name);
	
	const char *type = "stat() failed";
	
	struct stat st;
	if (stat((char*)info->niPath, &st) == 0)
	{
		if (S_ISDIR(st.st_mode))
		{
			type = "Directory";
			info->niFlags |= GWM_NODE_HASCHLD;
		}
		else
		{
			type = "File";
		};
	};
	
	info->niValues[1] = strdup(type);
	return 0;
};

static void fsCloseNode(void *context)
{
	FSNode *node = (FSNode*) context;
	closedir(node->dirp);
	free(node);
};

GWMTreeEnum gwmTreeFilesystem = {
	.teSize =				sizeof(GWMTreeEnum),
	.tePathSize =				PATH_MAX,
	.teNumCols =				sizeof(fsCols) / sizeof(const char*),
	.teGetColCaption =			fsGetColCaption,
	.teOpenNode =				fsOpenNode,
	.teGetNext =				fsGetNext,
	.teCloseNode =				fsCloseNode
};
