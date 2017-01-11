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

#include <glidix/pageinfo.h>
#include <glidix/mutex.h>
#include <glidix/memory.h>
#include <glidix/physmem.h>
#include <glidix/string.h>

static Mutex piLock;
static PageInfoNode piRoot;

void piInit()
{
	mutexInit(&piLock);
	memset(&piRoot, 0, sizeof(PageInfoNode));
};

uint64_t piNew(uint64_t flags)
{
	uint64_t frame = phmAllocZeroFrame();
	if (frame == 0) return 0;
	
	mutexLock(&piLock);
	uint64_t i;
	PageInfoNode *node = &piRoot;
	for (i=0; i<3; i++)
	{
		uint64_t index = (frame >> (9 * (3-i))) & 0x1F;
		if (node->entries[index] == 0)
		{
			PageInfoNode *newNode = NEW(PageInfoNode);
			memset(newNode, 0, sizeof(PageInfoNode));
			
			node->entries[index] = (uint64_t) newNode;
			node = newNode;
		}
		else
		{
			node = (PageInfoNode*) node->entries[index];
		};
	};
	
	uint64_t index = frame & 0x1F;
	node->entries[index] = 1UL | flags;
	mutexUnlock(&piLock);
	
	return frame;
};

void piIncref(uint64_t frame)
{
	__sync_fetch_and_add(
		&piRoot.branches[(frame>>27)&0x1F]->branches[(frame>>18)&0x1F]->branches[(frame>>9)&0x1F]->entries[frame&0x1F],
		1);
};

void piDecref(uint64_t frame)
{
	uint64_t newEnt = __sync_add_and_fetch(
		&piRoot.branches[(frame>>27)&0x1F]->branches[(frame>>18)&0x1F]->branches[(frame>>9)&0x1F]->entries[frame&0x1F],
		-1);
	
	if ((newEnt & 0xFFFFFFFF) == 0)
	{
		if ((newEnt & PI_CACHE) == 0)
		{
			phmFreeFrame(frame);
		};
	};
};

void piMarkDirty(uint64_t frame)
{
	__sync_fetch_and_or(
		&piRoot.branches[(frame>>27)&0x1F]->branches[(frame>>18)&0x1F]->branches[(frame>>9)&0x1F]->entries[frame&0x1F],
		PI_DIRTY);
};

void piMarkAccessed(uint64_t frame)
{
	__sync_fetch_and_or(
		&piRoot.branches[(frame>>27)&0x1F]->branches[(frame>>18)&0x1F]->branches[(frame>>9)&0x1F]->entries[frame&0x1F],
		PI_DIRTY);
};
