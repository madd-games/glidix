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

#include <glidix/ftree.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/mutex.h>
#include <glidix/pageinfo.h>
#include <glidix/pagetab.h>
#include <glidix/sched.h>
#include <glidix/console.h>

static Mutex ftMtx;
static FileTree* ftFirst;
static FileTree* ftLast;

void ftInit()
{
	mutexInit(&ftMtx);
	ftFirst = ftLast = NULL;
};

FileTree* ftCreate(int flags)
{
	FileTree *ft = NEW(FileTree);
	ft->refcount = 1;
	ft->flags = flags;
	memset(&ft->top, 0, sizeof(FileNode));
	semInit(&ft->lock);
	ft->data = NULL;
	ft->load = NULL;
	ft->flush = NULL;
	ft->update = NULL;
	ft->size = 0;
	
	mutexLock(&ftMtx);
	if (ftLast == NULL)
	{
		ft->prev = ft->next = NULL;
		ftFirst = ftLast = ft;
	}
	else
	{
		ft->prev = ftLast;
		ft->next = NULL;
		ftLast->next = ft;
		ftLast = ft;
	};
	mutexUnlock(&ftMtx);
	
	return ft;
};

void ftUp(FileTree *ft)
{
	__sync_fetch_and_add(&ft->refcount, 1);
};

static void deleteTree(int level, FileNode *node)
{
	int i;
	for (i=0; i<16; i++)
	{
		if (level == 12)
		{
			if (node->entries[i] != 0) piUncache(node->entries[i]);
		}
		else
		{
			FileNode *subnode = node->nodes[i];
			if (subnode != NULL)
			{
				deleteTree(level+1, subnode);
			};
			kfree(subnode);
		};
	};
};

void ftDown(FileTree *ft)
{
	if (__sync_add_and_fetch(&ft->refcount, -1) == 0)
	{
		if (ft->flags & FT_ANON)
		{
			// uncache all pages
			deleteTree(0, &ft->top);
			kfree(ft);
		};
	};
};

static uint64_t getPageUnlocked(FileTree *ft, off_t pos)
{
	FileNode *node = &ft->top;
	int i;
	for (i=0; i<12; i++)
	{
		uint64_t ent = (pos >> (12 + 4 * (12 - i))) & 0xF;
		
		if (node->nodes[ent] == NULL)
		{
			FileNode *newNode = NEW(FileNode);
			memset(newNode, 0, sizeof(FileNode));
			
			node->nodes[ent] = newNode;
			node = newNode;
		}
		else
		{
			node = node->nodes[ent];
		};
	};
	
	// load the page if necessary
	uint64_t pageIndex = (pos >> 12) & 0xF;
	if (node->entries[pageIndex] == 0)
	{
		// first let's try loading
		uint8_t pagebuf[0x1000];
		memset(pagebuf, 0, 0x1000);
		
		if ((ft->flags & FT_ANON) == 0)
		{
			if (ft->load == NULL)
			{
				return 0;
			};
		
			if (ft->load(ft, pos, pagebuf) != 0)
			{
				return 0;
			};
		};
		
		uint64_t frame = piNew(PI_CACHE);
		if (frame == 0)
		{
			return 0;
		};
		
		frameWrite(frame, pagebuf);
		
		node->entries[pageIndex] = frame;
		return frame;
	}
	else
	{
		uint64_t frame = node->entries[pageIndex];
		piIncref(frame);
		return frame;
	};
};

uint64_t ftGetPage(FileTree *ft, off_t pos)
{
	semWait(&ft->lock);
	uint64_t frame = getPageUnlocked(ft, pos);
	semSignal(&ft->lock);
	return frame;
};

ssize_t ftRead(FileTree *ft, void *buffer, size_t size, off_t pos)
{
	semWait(&ft->lock);
	
	if (pos >= ft->size)
	{
		semSignal(&ft->lock);
		return 0;
	};
	
	if ((pos+size) >= ft->size)
	{
		size = ft->size - pos;
	};
	
	ssize_t sizeRead = 0;
	uint8_t *put = (uint8_t*) buffer;
	
	while (size > 0)
	{
		size_t sizeToRead = size;
		size_t maxReadable = 0x1000 - (pos & 0xFFF);
		if (sizeToRead > maxReadable) sizeToRead = maxReadable;
		
		uint64_t frame = getPageUnlocked(ft, pos & ~0xFFF);
		if (frame == 0)
		{
			break;
		}
		else
		{
			uint64_t old = mapTempFrame(frame);
			memcpy(put, (char*) tmpframe() + (pos & 0xFFF), sizeToRead);
			mapTempFrame(old);
			piMarkAccessed(frame);
			piDecref(frame);
		};
		
		put += sizeToRead;
		sizeRead += sizeToRead;
		pos += sizeToRead;
		size -= sizeToRead;
	};
	
	semSignal(&ft->lock);
	return sizeRead;
};

ssize_t ftWrite(FileTree *ft, const void *buffer, size_t size, off_t pos)
{
	semWait(&ft->lock);
	
	if ((pos+size) >= ft->size)
	{
		ft->size = pos + size;
		if (ft->update != NULL) ft->update(ft);
	};
	
	ssize_t sizeWritten = 0;
	const uint8_t *scan = (const uint8_t*) buffer;
	
	while (size > 0)
	{
		size_t sizeToWrite = size;
		size_t maxWriteable = 0x1000 - (pos & 0xFFF);
		if (sizeToWrite > maxWriteable) sizeToWrite = maxWriteable;
		
		uint64_t frame = getPageUnlocked(ft, pos & ~0xFFF);
		if (frame == 0)
		{
			break;
		}
		else
		{
			uint64_t old = mapTempFrame(frame);
			memcpy((char*) tmpframe() + (pos & 0xFFF), scan, sizeToWrite);
			mapTempFrame(old);
			piMarkAccessed(frame);
			piMarkDirty(frame);
			piDecref(frame);
		};
		
		scan += sizeToWrite;
		sizeWritten += sizeToWrite;
		pos += sizeToWrite;
		size -= sizeToWrite;
	};
	
	semSignal(&ft->lock);
	return sizeWritten;
};

int ftTruncate(FileTree *ft, size_t size)
{
	semWait(&ft->lock);
	if (ft->flags & FT_READONLY)
	{
		semSignal(&ft->lock);
		return -1;
	};
	
	if (size < ft->size)
	{
		size_t pos;
		for (pos=size+0xFFF; pos<ft->size; pos+=0x1000)
		{
			FileNode *node = &ft->top;
			int i;
			int foundNode = 1;
			for (i=0; i<12; i++)
			{
				uint64_t ent = (pos >> (12 + 4 * (12 - i))) & 0xF;
		
				if (node->nodes[ent] == NULL)
				{
					foundNode = 0;
					break;
				}
				else
				{
					node = node->nodes[ent];
				};
			};
			
			if (foundNode)
			{
				uint64_t pageIndex = (pos >> 12) & 0xF;
				if (node->entries[pageIndex] != 0)
				{
					piUncache(node->entries[pageIndex]);
					node->entries[pageIndex] = 0;
				};
			};
		};
	};
	
	ft->size = size;
	semSignal(&ft->lock);
	return 0;
};

void ftUncache(FileTree *ft)
{
	mutexLock(&ftMtx);
	if (ft->prev != NULL) ft->prev->next = ft->next;
	if (ftFirst == ft) ftFirst = ft->next;
	if (ft->next != NULL) ft->next->prev = ft->prev;
	if (ftLast == ft) ftLast = ft->prev;
	mutexUnlock(&ftMtx);
	
	ft->flags |= FT_ANON;
};
