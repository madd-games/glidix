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

#include <glidix/util/qfile.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>

static QFileEntry *nextEntry(Inode *inode)
{
	QFileEntry *entry = (QFileEntry*) inode->fsdata;
	if (entry != NULL)
	{
		inode->fsdata = entry->next;
		// remove the link to prevent disasters
		entry->next = NULL;
	};
	
	return entry;
};

void qfileFree(Inode *inode)
{
	while (1)
	{
		QFileEntry *entry = nextEntry(inode);
		if (entry == NULL) break;
		kfree(entry);
	};
};

ssize_t qfileRead(Inode *inode, File *fp, void *buffer, size_t size, off_t off)
{
	QFileEntry *entry = nextEntry(inode);
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
	Inode *inode = vfsCreateInode(NULL, VFS_MODE_FIFO | 0400);
	inode->fsdata = head;
	inode->pread = qfileRead;
	inode->free = qfileFree;
	
	InodeRef iref;
	iref.top = NULL;
	iref.inode = inode;
	
	return vfsOpenInode(iref, O_RDONLY, NULL);	/* no error can happen when the inode doesn't implement open() */
};
