/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
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

static Spinlock  physmemLock;
static uint64_t *frameStack;
static uint64_t  frameStackPointer;
static uint64_t  nextFrame;
static uint64_t  numSystemFrames;

void initPhysMem(uint64_t numPages)
{
	frameStackPointer = 0;
	nextFrame = 0x200;
	numSystemFrames = numPages;
	spinlockRelease(&physmemLock);
};

void initPhysMem2()
{
	frameStack = kmalloc(8*numSystemFrames);
};

uint64_t phmAllocFrame()
{
	spinlockAcquire(&physmemLock);
	uint64_t out;
	if (frameStackPointer == 0)
	{
		if (nextFrame == numSystemFrames)
		{
			heapDump();
			panic("out of physical memory!\n");
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
	spinlockAcquire(&physmemLock);
	frameStack[frameStackPointer++] = frame;
	spinlockRelease(&physmemLock);
};
