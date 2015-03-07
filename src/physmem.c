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

#include <glidix/physmem.h>
#include <glidix/memory.h>
#include <glidix/spinlock.h>
#include <glidix/common.h>

static Spinlock			physmemLock;
static uint64_t*		frameStack;
static uint64_t			frameStackPointer;
static uint64_t			nextFrame;
static uint64_t			numSystemFrames;
static MultibootMemoryMap*	memoryMap;
static uint64_t			memoryMapEnd;

static int isUseableMemory(MultibootMemoryMap *mmap)
{
	if (mmap->type != 1) return 0;
	if (mmap->baseAddr & 0xFFF) return 0;
	return 1;
};

static int isFrameInArea(MultibootMemoryMap *mmap, uint64_t frame)
{
	uint64_t mmapStartFrame = mmap->baseAddr / 0x1000;
	uint64_t mmapNumFrames = mmap->len / 0x1000;
	uint64_t mmapEndFrame = mmapStartFrame + mmapNumFrames;

	if ((frame >= mmapStartFrame) || (frame < mmapEndFrame))
	{
		return 1;
	}
	else
	{
		return 0;
	};
};

void initPhysMem(uint64_t numPages, MultibootMemoryMap *mmap, uint64_t mmapEnd)
{
	frameStackPointer = 0;
	nextFrame = 0x200;
	numSystemFrames = numPages;

	while ((uint64_t)mmap < mmapEnd)
	{
		if (isUseableMemory(mmap))
		{
			if (isFrameInArea(mmap, 0x200))
			{
				break;
			};
		};

		mmap = (MultibootMemoryMap*) ((uint64_t) mmap + mmap->size + 4);
	};

	if ((uint64_t)mmap >= mmapEnd)
	{
		panic("no RAM addresses detected!");
	};

	memoryMap = mmap;
	memoryMapEnd = mmapEnd;
	spinlockRelease(&physmemLock);
};

void initPhysMem2()
{
	frameStack = kmalloc(8*numSystemFrames);
};

static void loadNextMemory()
{
	do
	{
		memoryMap = (MultibootMemoryMap*) ((uint64_t) memoryMap + memoryMap->size + 4);
		if (isUseableMemory(memoryMap))
		{
			nextFrame = memoryMap->baseAddr / 0x1000;
		};
	} while ((uint64_t) memoryMap < memoryMapEnd);

	if ((uint64_t) memoryMap >= memoryMapEnd)
	{
		heapDump();
		panic("out of physical memory");
	};
};

uint64_t phmAllocFrame()
{
	spinlockAcquire(&physmemLock);
	uint64_t out;
	if (frameStackPointer == 0)
	{
		uint64_t mmapStartFrame = memoryMap->baseAddr / 0x1000;
		uint64_t mmapNumFrames = memoryMap->len / 0x1000;
		uint64_t mmapEndFrame = mmapStartFrame + mmapNumFrames;

		if (mmapEndFrame == nextFrame)
		{
			loadNextMemory();
		};

		out = nextFrame;
		nextFrame++;
	}
	else
	{
		out = frameStack[--frameStackPointer];
	};

	spinlockRelease(&physmemLock);
	return out;
};

void phmFreeFrame(uint64_t frame)
{
	if (frame < 0x200) panic("attempted to free frame %d (below 2MB)", frame);

	spinlockAcquire(&physmemLock);
	frameStack[frameStackPointer++] = frame;
	spinlockRelease(&physmemLock);
};
