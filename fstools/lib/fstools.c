/*
	Glidix Filesystem Tools

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
#include <unistd.h>
#include <fstools.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

//static FSMimeType *firstType = NULL;
//static FSMimeType *lastType = NULL;

static FSMimeType *textPlain;
static FSMimeType *octetStream;

static FSMimeType *inodeDir;
static FSMimeType *inodeBlockdev;
static FSMimeType *inodeChardev;
static FSMimeType *inodeFifo;
static FSMimeType *inodeSocket;

static FSMimeType *specialType(const char *name)
{
	FSMimeType *type = (FSMimeType*) malloc(sizeof(FSMimeType));
	type->next = NULL;
	strcpy(type->mimename, name);
	type->filenames = NULL;
	type->numFilenames = 0;
	type->magics = NULL;
	type->numMagics = 0;
	
	return type;
};

void fsInit()
{
	/**
	 * Some special types.
	 */
	textPlain = specialType("text/plain");
	octetStream = specialType("application/octet-stream");
	inodeDir = specialType("inode/directory");
	inodeBlockdev = specialType("inode/blockdevice");
	inodeChardev = specialType("inode/chardevice");
	inodeFifo = specialType("inode/fifo");
	inodeSocket = specialType("inode/socket");
	
	// TODO: actually load the database
};

void fsQuit()
{
};

static int isTextFile(const char *path, struct stat *st)
{
	if (st->st_size > 6*1024*1024)
	{
		// text files larger than 6MB are improbable
		return 0;
	};
	
	int fd = open(path, O_RDONLY);
	if (fd == -1) return 0;
	
	char buffer[8*1024];
	
	while (1)
	{
		ssize_t sizeNow = read(fd, buffer, 8*1024);
		if (sizeNow == -1)
		{
			close(fd);
			return 0;
		};
		
		if (sizeNow == 0) break;
		
		ssize_t i;
		for (i=0; i<sizeNow; i++)
		{
			if (buffer[i] == 0)
			{
				close(fd);
				return 0;
			};
		};
	};
	
	close(fd);
	return 1;
};

FSMimeType* fsGetType(const char *filename)
{
	//const char *basename = filename;
	//const char *slashPos = strrchr(filename, '/');
	//if (slashPos != NULL)
	//{
	//	basename = slashPos + 1;
	//};
	
	struct stat st;
	if (stat(filename, &st) != 0)
	{
		return octetStream;
	};
	
	if (S_ISBLK(st.st_mode))
	{
		return inodeBlockdev;
	}
	else if (S_ISCHR(st.st_mode))
	{
		return inodeChardev;
	}
	else if (S_ISDIR(st.st_mode))
	{
		return inodeDir;
	}
	else if (S_ISFIFO(st.st_mode))
	{
		return inodeFifo;
	}
	else if (S_ISSOCK(st.st_mode))
	{
		return inodeSocket;
	}
	else
	{
		// TODO: look up patterns etc
		
		if (isTextFile(filename, &st))
		{
			return textPlain;
		}
		else
		{
			return octetStream;
		};
	};
};
