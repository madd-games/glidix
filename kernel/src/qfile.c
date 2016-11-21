/*
	Glidix kernel

	Copyright (c) 2014-2016, Madd Games.
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

#include <glidix/qfile.h>
#include <glidix/memory.h>
#include <glidix/string.h>

static QFileEntry *nextEntry(File *fp)
{
	QFileEntry *entry = (QFileEntry*) fp->fsdata;
	if (entry != NULL)
	{
		fp->fsdata = entry->next;
		// remove the link to prevent disasters
		entry->next = NULL;
	};
	
	return entry;
};

void qfileClose(File *fp)
{
	while (1)
	{
		QFileEntry *entry = nextEntry(fp);
		if (entry == NULL) break;
		kfree(entry);
	};
};

ssize_t qfileRead(File *fp, void *buffer, size_t size)
{
	QFileEntry *entry = nextEntry(fp);
	if (entry == 0) return 0;
	
	if (size > entry->size)
	{
		size = entry->size;
	};
	
	memcpy(buffer, entry->data, size);
	kfree(entry);
	return (ssize_t) size;
};

File *qfileCreate(QFileEntry *head)
{
	File *fp = (File*) kmalloc(sizeof(File));
	memset(fp, 0, sizeof(File));
	fp->oflag = O_RDONLY;
	fp->refcount = 1;
	fp->fsdata = head;
	fp->read = qfileRead;
	fp->close = qfileClose;
	
	return fp;
};
