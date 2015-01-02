/*
	Glidix kernel

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

#include <glidix/ftab.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/console.h>
#include <glidix/common.h>

FileTable *ftabCreate()
{
	FileTable *ftab = (FileTable*) kmalloc(sizeof(FileTable));
	ftab->refcount = 1;
	memset(ftab->entries, 0, sizeof(void*)*MAX_OPEN_FILES);		// all description pointers start as NULL
	spinlockRelease(&ftab->spinlock);

	return ftab;
};

void ftabUpref(FileTable *ftab)
{
	spinlockAcquire(&ftab->spinlock);
	ftab->refcount++;
	spinlockRelease(&ftab->spinlock);
};

void ftabDownref(FileTable *ftab)
{
	spinlockAcquire(&ftab->spinlock);
	ftab->refcount--;
	if (ftab->refcount == 0)
	{
		int i;
		for (i=0; i<MAX_OPEN_FILES; i++)
		{
			File *fp = ftab->entries[i];
			if (fp != NULL)
			{
				if (fp->close != NULL) fp->close(fp);
				kfree(fp);
			};
		};

		kfree(ftab);
	}
	else
	{
		spinlockRelease(&ftab->spinlock);
	};
};

FileTable *ftabDup(FileTable *old)
{
	FileTable *new = ftabCreate();

	spinlockAcquire(&old->spinlock);
	int i;
	for (i=0; i<MAX_OPEN_FILES; i++)
	{
		File *fold = old->entries[i];
		if (fold != NULL)
		{
			if (fold->dup != NULL)
			{
				File *fnew = (File*) kmalloc(sizeof(File));
				memset(fnew, 0, sizeof(File));

				if (fold->dup(fold, fnew, sizeof(File)) != 0)
				{
					// failed
					kfree(fnew);
				}
				else
				{
					new->entries[i] = fnew;
				};
			};
		};
	};
	spinlockRelease(&old->spinlock);

	return new;
};
