/*
	Glidix kernel

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

#include <glidix/ftab.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/common.h>

#define	FILE_RESERVE				((File*)1)

FileTable *ftabCreate()
{
	FileTable *ftab = (FileTable*) kmalloc(sizeof(FileTable));
	ftab->refcount = 1;
	memset(ftab->fps, 0, sizeof(void*)*MAX_OPEN_FILES);		// all description pointers start as NULL
	semInit(&ftab->lock);

	return ftab;
};

void ftabUpref(FileTable *ftab)
{
	__sync_fetch_and_add(&ftab->refcount, 1);
};

void ftabDownref(FileTable *ftab)
{
	if (__sync_add_and_fetch(&ftab->refcount, -1) == 0)
	{
		int i;
		for (i=0; i<MAX_OPEN_FILES; i++)
		{
			File *fp = ftab->fps[i];
			if (fp != NULL)
			{
				vfsClose(fp);
			};
		};

		kfree(ftab);
	};
};

FileTable *ftabDup(FileTable *old)
{
	FileTable *new = ftabCreate();

	semWait(&old->lock);
	int i;
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		File *fold = old->fps[i];
		if (fold != NULL)
		{
			vfsDup(fold);
			new->fps[i] = fold;
		};
	};
	semSignal(&old->lock);

	return new;
};

File* ftabGet(FileTable *ftab, int fd)
{
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		return NULL;
	};
	
	semWait(&ftab->lock);
	File *fp = ftab->fps[fd];
	if ((fp == NULL) || (fp == FILE_RESERVE))
	{
		semSignal(&ftab->lock);
		return NULL;
	};
	
	vfsDup(fp);
	semSignal(&ftab->lock);
	
	return fp;
};

int ftabAlloc(FileTable *ftab)
{
	semWait(&ftab->lock);
	
	int fd;
	for (fd=0; fd<MAX_OPEN_FILES; fd++)
	{
		if (ftab->fps[fd] == NULL)
		{
			ftab->fps[fd] = FILE_RESERVE;
			semSignal(&ftab->lock);
			return fd;
		};
	};
	
	semSignal(&ftab->lock);
	return -1;
};

void ftabSet(FileTable *ftab, int fd, File *fp)
{
	semWait(&ftab->lock);
	if (ftab->fps[fd] != FILE_RESERVE)
	{
		panic("ftabSet called on a non-reserved descriptor");
	};
	
	ftab->fps[fd] = fp;
	semSignal(&ftab->lock);
};

int ftabClose(FileTable *ftab, int fd)
{
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		return EBADF;
	};
	
	semWait(&ftab->lock);
	if (ftab->fps[fd] == FILE_RESERVE)
	{
		semSignal(&ftab->lock);
		return EBADF;
	};
	
	File *old = ftab->fps[fd];
	ftab->fps[fd] = NULL;
	semSignal(&ftab->lock);
	
	if (old != NULL) vfsClose(old);
	else return EBADF;
	
	return 0;
};

int ftabPut(FileTable *ftab, int fd, File *fp)
{
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		return EBADF;
	};
	
	semWait(&ftab->lock);
	File *old = ftab->fps[fd];
	if (old == FILE_RESERVE)
	{
		semSignal(&ftab->lock);
		return EBUSY;
	};
	
	ftab->fps[fd] = fp;
	semSignal(&ftab->lock);
	
	if (old != NULL) vfsClose(old);
	return 0;
};

void ftabCloseOnExec(FileTable *ftab)
{
	semWait(&ftab->lock);
	
	int i;
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		File *fp = ftab->fps[i];
		if (fp != NULL)
		{
			if (fp->oflag & O_CLOEXEC)
			{
				ftab->fps[i] = NULL;
				vfsClose(fp);
			};
		};
	};
	
	semSignal(&ftab->lock);
};
