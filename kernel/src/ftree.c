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

void ftDown(FileTree *ft)
{
	if (__sync_add_and_fetch(&ft->refcount, -1) == 0)
	{
		if (ft->flags & FT_ANON)
		{
			// all pages assumed to be freed!
			kfree(ft);
		};
	};
};

uint64_t ftGetPage(FileTree *ft, off_t pos)
{
	semWait(&ft->lock);
	
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
		
		if (ft->load == NULL)
		{
			semSignal(&ft->lock);
			return 0;
		};
		
		memset(pagebuf, 0, 0x1000);
		if (ft->load(ft, pos, pagebuf) != 0)
		{
			semSignal(&ft->lock);
			return 0;
		};
		
		uint64_t flags = PI_CACHE;
		if (ft->flags & FT_ANON) flags = 0;
		
		uint64_t frame = piNew(flags);
		if (frame == 0)
		{
			semSignal(&ft->lock);
			return 0;
		};
		
		frameWrite(frame, pagebuf);
		
		node->entries[pageIndex] = frame;
		semSignal(&ft->lock);
		return frame;
	}
	else
	{
		uint64_t frame = node->entries[pageIndex];
		piIncref(frame);
		semSignal(&ft->lock);
		return frame;
	};
};
