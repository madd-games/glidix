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

#include <glidix/thread/ftab.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>
#include <glidix/display/console.h>
#include <glidix/util/common.h>
#include <glidix/thread/sched.h>

#define	FILE_RESERVE				((File*)1)

FileTable *ftabCreate()
{
	FileTable *ftab = (FileTable*) kmalloc(sizeof(FileTable));
	ftab->refcount = 1;
	memset(ftab->list, 0, sizeof(ftab->list));		// all description pointers start as NULL
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
			File *fp = ftab->list[i].fp;
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
		File *fold = old->list[i].fp;
		if (fold != NULL)
		{
			vfsDup(fold);
			new->list[i].fp = fold;
			new->list[i].flags = old->list[i].flags;
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
	File *fp = ftab->list[fd].fp;
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
		if (ftab->list[fd].fp == NULL)
		{
			ftab->list[fd].fp = FILE_RESERVE;
			semSignal(&ftab->lock);
			return fd;
		};
	};
	
	semSignal(&ftab->lock);
	return -1;
};

void ftabSet(FileTable *ftab, int fd, File *fp, int flags)
{
	semWait(&ftab->lock);
	if (ftab->list[fd].fp != FILE_RESERVE)
	{
		panic("ftabSet called on a non-reserved descriptor");
	};
	
	ftab->list[fd].fp = fp;
	ftab->list[fd].flags = flags;
	semSignal(&ftab->lock);
};

int ftabClose(FileTable *ftab, int fd)
{
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		return EBADF;
	};
	
	semWait(&ftab->lock);
	if (ftab->list[fd].fp == FILE_RESERVE)
	{
		semSignal(&ftab->lock);
		return EBADF;
	};
	
	File *old = ftab->list[fd].fp;
	ftab->list[fd].fp = NULL;
	semSignal(&ftab->lock);
	
	if (old != NULL)
	{
		vfsClose(old);
	}
	else return EBADF;
	
	return 0;
};

int ftabPut(FileTable *ftab, int fd, File *fp, int flags)
{
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		return EBADF;
	};
	
	semWait(&ftab->lock);
	File *old = ftab->list[fd].fp;
	if (old == FILE_RESERVE)
	{
		semSignal(&ftab->lock);
		return EBUSY;
	};
	
	ftab->list[fd].fp = fp;
	ftab->list[fd].flags = flags;
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
		File *fp = ftab->list[i].fp;
		if (fp != NULL)
		{
			if (ftab->list[i].flags & FD_CLOEXEC)
			{
				ftab->list[i].fp = NULL;
				vfsClose(fp);
			};
		};
	};
	
	semSignal(&ftab->lock);
};

int ftabGetFlags(FileTable *ftab, int fd)
{
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		return -1;
	};

	semWait(&ftab->lock);
	int flags = ftab->list[fd].flags;
	semSignal(&ftab->lock);
	
	return flags;
};

int ftabSetFlags(FileTable *ftab, int fd, int flags)
{
	if ((fd < 0) || (fd >= MAX_OPEN_FILES))
	{
		return EBADF;
	};
	
	semWait(&ftab->lock);
	ftab->list[fd].flags = flags;
	semSignal(&ftab->lock);
	
	return 0;
};
