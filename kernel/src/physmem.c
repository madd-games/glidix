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

#include <glidix/physmem.h>
#include <glidix/memory.h>
#include <glidix/spinlock.h>
#include <glidix/common.h>
#include <glidix/console.h>
#include <glidix/string.h>
#include <glidix/isp.h>

/**
 * The next frame to return if we are allocating using placement. This is done before
 * we can allocate the frame bitmap on the heap.
 */
static uint64_t 		placementFrame;

/**
 * Memory map from the bootloader, used to find RAM.
 */
static MultibootMemoryMap*	memoryMap;
static MultibootMemoryMap*	memoryMapStart;
static uint64_t			memoryMapEnd;

/**
 * Total number of frames in the system, and the frame bitmap.
 */
static uint64_t			numSystemFrames;
static uint8_t*			frameBitmap = NULL;

/**
 * Lowest frame that could be free (i.e. one that was free during init).
 */
static uint64_t			lowestFreeFrame;

static Spinlock			physmemLock;

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

void initPhysMem(uint64_t numPages, MultibootMemoryMap *mmap, uint64_t mmapEnd, uint64_t endAddr)
{
	placementFrame = endAddr >> 12;
	numSystemFrames = numPages;
	spinlockRelease(&physmemLock);
	
	memoryMap = mmap;
	memoryMapStart = mmap;
	memoryMapEnd = mmapEnd;
	
	while ((uint64_t)mmap < mmapEnd)
	{
		if (isUseableMemory(mmap))
		{
			if (isFrameInArea(mmap, placementFrame))
			{
				break;
			};
		};

		mmap = (MultibootMemoryMap*) ((uint64_t) mmap + mmap->size + 4);
	};

	if ((uint64_t)mmap >= mmapEnd)
	{
		FAILED();
		panic("no RAM addresses detected!");
	};
};

static void phmClearFrame(uint64_t frame)
{
	uint64_t byte = frame/8;
	uint64_t bit = frame%8;
	atomic_and8(&frameBitmap[byte], ~(1 << bit));
};

/**
 * Returns 0 if the frame was successfully acquired.
 */
static int phmTryFrame(uint64_t frame)
{
	uint64_t byte = frame/8;
	int bit = (int) (frame%8);
	return atomic_test_and_set8(&frameBitmap[byte], bit);
};

static uint64_t phmAllocSingle()
{
	uint64_t i;
	uint64_t limit = numSystemFrames;
	
	// find the first group of 64 frames that isn't all used
	uint64_t *bitmap64 = (uint64_t*) frameBitmap;
	uint64_t startAt = 0;
	for (i=(lowestFreeFrame>>6); i<(limit>>6); i++)
	{
		if (bitmap64[i] != 0xFFFFFFFFFFFFFFFF)
		{
			startAt = i << 6;
			break;
		};
	};
	
	if (startAt == 0)
	{
		panic("out of physical memory!");
	};
	
	for (i=startAt; i<limit; i++)
	{
		if (phmTryFrame(i) == 0)
		{
			return i;
		};
	};
	
	panic("out of physical memory!");
	return 0;
};

static uint64_t phmAlloc8()
{
	uint64_t i;
	uint64_t numGroups = numSystemFrames/8;
	
	uint8_t *bitmap8 = frameBitmap;
	for (i=(lowestFreeFrame>>3); i<(numGroups>>3); i++)
	{
		if (atomic_compare_and_swap8(&bitmap8[i], 0, 0xFF) == 0)
		{
			// just claimed the 8 frames at this location
			return i << 3;
		};
	};
	
	panic("out of physical memory!");
	return 0;
};

static uint64_t phmAlloc16()
{
	uint64_t i;
	uint64_t numGroups = numSystemFrames/16;
	
	uint16_t *bitmap16 = (uint16_t*) frameBitmap;
	for (i=(lowestFreeFrame>>4); i<(numGroups>>4); i++)
	{
		if (atomic_compare_and_swap16(&bitmap16[i], 0, 0xFFFF) == 0)
		{
			// just claimed the 16 frames at this location
			return i << 4;
		};
	};
	
	panic("out of physical memory!");
	return 0;
};

static uint64_t phmAlloc32()
{
	uint64_t i;
	uint64_t numGroups = numSystemFrames/32;
	
	uint32_t *bitmap32 = (uint32_t*) frameBitmap;
	for (i=(lowestFreeFrame>>5); i<(numGroups>>5); i++)
	{
		if (atomic_compare_and_swap32(&bitmap32[i], 0, 0xFFFFFFFF) == 0)
		{
			// just claimed the 32 frames at this location
			return i << 5;
		};
	};

	panic("out of physical memory!");
	return 0;
};

static uint64_t phmAlloc64()
{
	uint64_t i;
	uint64_t numGroups = numSystemFrames/64;
	
	uint64_t *bitmap64 = (uint64_t*) frameBitmap;
	for (i=(lowestFreeFrame>>6); i<(numGroups>>6); i++)
	{
		if (atomic_compare_and_swap64(&bitmap64[i], 0, 0xFFFFFFFFFFFFFFFF) == 0)
		{
			// just claimed the 64 frames at this location
			return i << 6;
		};
	};
	
	panic("out of physical memory!");
	return 0;
};

void initPhysMem2()
{
	frameBitmap = (uint8_t*) kmalloc(numSystemFrames/8+1);

	// We mark all frames used, and then free the ones that belong to the normal RAM
	// ranges. This way, phmAllocFrame() will never return memory holes.
	memset(frameBitmap, 0xFF, numSystemFrames/8+1);
	
	MultibootMemoryMap *mmap = memoryMapStart;
	while ((uint64_t) mmap < memoryMapEnd)
	{
		if (isUseableMemory(mmap))
		{
			uint64_t startFrame = mmap->baseAddr / 0x1000;
			uint64_t endFrame = (mmap->baseAddr + mmap->len) / 0x1000 + 1;
			
			uint64_t i;
			for (i=startFrame; i<endFrame; i++)
			{
				if (i >= placementFrame)
				{
					phmClearFrame(i);
				};
			};
		};
		
		mmap = (MultibootMemoryMap*) ((uint64_t) mmap + mmap->size + 4);
	};
	
	uint64_t i;
	for (i=0; i<(numSystemFrames/8); i++)
	{
		if (frameBitmap[i] != 0xFF)
		{
			lowestFreeFrame = i * 8;
			break;
		};
	};
};

static void loadNextMemory()
{
	do
	{
		memoryMap = (MultibootMemoryMap*) ((uint64_t) memoryMap + memoryMap->size + 4);
		if (isUseableMemory(memoryMap))
		{
			placementFrame = memoryMap->baseAddr / 0x1000;
			break;
		};
	} while ((uint64_t) memoryMap < memoryMapEnd);

	if ((uint64_t) memoryMap >= memoryMapEnd)
	{
		panic("out of physical memory!");
	};
};

uint64_t phmAllocFrame()
{
	uint64_t out;
	if (frameBitmap == NULL)
	{
		spinlockAcquire(&physmemLock);
		uint64_t mmapStartFrame = memoryMap->baseAddr / 0x1000;
		uint64_t mmapNumFrames = memoryMap->len / 0x1000;
		uint64_t mmapEndFrame = mmapStartFrame + mmapNumFrames;

		if (mmapEndFrame == placementFrame)
		{
			loadNextMemory();
		};

		out = placementFrame;
		placementFrame++;
		spinlockRelease(&physmemLock);
	}
	else
	{
		out = phmAllocSingle();
	};
	
	return out;
};

void phmFreeFrameEx(uint64_t start, uint64_t count)
{
	uint64_t i;
	for (i=0; i<count; i++)
	{
		phmFreeFrame(start+i);
	};
};

uint64_t phmAllocZeroFrame()
{
	uint64_t frame = phmAllocFrame();
	ispLock();
	ispSetFrame(frame);
	ispZero();
	ispUnlock();
	return frame;
};

uint64_t phmAllocFrameEx(uint64_t count, int flags)
{
	if (count == 0) return 0;
	if (count == 1) return phmAllocSingle();
	
	uint64_t actuallyAllocated;
	uint64_t base;
	if (count > 64)
	{
		panic("attempted to allocate more than 64 consecutive frames (%d)\n", (int) count);
	}
	else if (count > 32)
	{
		actuallyAllocated = 64;
		base = phmAlloc64();
	}
	else if (count > 16)
	{
		actuallyAllocated = 32;
		base = phmAlloc32();
	}
	else if (count > 8)
	{
		actuallyAllocated = 16;
		base = phmAlloc16();
	}
	else
	{
		actuallyAllocated = 8;
		base = phmAlloc8();
	};
	
	// free the unneeded frames
	phmFreeFrameEx(base + count, actuallyAllocated - count);
	return base;
};

void phmFreeFrame(uint64_t frame)
{
	phmClearFrame(frame);
};
