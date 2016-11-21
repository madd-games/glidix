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

#include <glidix/dma.h>
#include <glidix/pagetab.h>
#include <glidix/spinlock.h>
#include <glidix/physmem.h>
#include <glidix/string.h>

static PAGE_ALIGN PDPT dmaPDPT;
static Spinlock dmaMemoryLock;

static void splitPageIndex(uint64_t index, uint64_t *dirIndex, uint64_t *tableIndex, uint64_t *pageIndex)
{
	*pageIndex = (index & 0x1FF);
	index >>= 9;
	*tableIndex = (index & 0x1FF);
	index >>= 9;
	*dirIndex = (index & 0x1FF);
};

static PTe *dmaGetPage(uint64_t index)
{
	uint64_t dirIndex, tableIndex, pageIndex;
	splitPageIndex(index, &dirIndex, &tableIndex, &pageIndex);
	uint64_t base = 0xFFFF838000000000;

	int makeDir=0, makeTable=0;

	PDPT *pdpt = (PDPT*) (base + 0x7FFFFFF000);
	if (!pdpt->entries[dirIndex].present)
	{
		pdpt->entries[dirIndex].present = 1;
		uint64_t frame = phmAllocFrame();
		pdpt->entries[dirIndex].pdPhysAddr = frame;
		pdpt->entries[dirIndex].rw = 1;
		refreshAddrSpace();
		makeDir = 1;
	};

	PD *pd = (PD*) (base + 0x7FFFE00000 + dirIndex * 0x1000);
	if (makeDir) memset(pd, 0, 0x1000);
	if (!pd->entries[tableIndex].present)
	{
		pd->entries[tableIndex].present = 1;
		uint64_t frame = phmAllocFrame();
		pd->entries[tableIndex].ptPhysAddr = frame;
		pd->entries[tableIndex].rw = 1;
		refreshAddrSpace();
		makeTable = 1;
	};

	PT *pt = (PT*) (base + 0x7FC0000000 + dirIndex * 0x200000 + tableIndex * 0x1000);
	if (makeTable) memset(pt, 0, 0x1000);
	return &pt->entries[pageIndex];
};

void dmaInit()
{
	spinlockRelease(&dmaMemoryLock);
	memset(&dmaPDPT, 0, sizeof(PDPT));
	dmaPDPT.entries[511].present = 1;
	dmaPDPT.entries[511].pdPhysAddr = ((uint64_t)&dmaPDPT - 0xFFFF800000000000) >> 12;
	dmaPDPT.entries[511].rw = 1;
	PML4 *pml4 = getPML4();
	pml4->entries[263].present = 1;
	pml4->entries[263].pdptPhysAddr = ((uint64_t)&dmaPDPT - 0xFFFF800000000000) >> 12;
	pml4->entries[263].rw = 1;
	refreshAddrSpace();
};

static int dmaCheckFreePages(uint64_t start, uint64_t count)
{
	uint64_t i;
	for (i=0; i<count; i++)
	{
		PTe *pte = dmaGetPage(start+i);
		if (pte->present) return 0;
	};
	
	return 1;
};

static uint64_t dmaAllocPages(uint64_t physStart, uint64_t count)
{
	uint64_t i;
	for (i=1; i<0x8000000; i++)
	{
		if (dmaCheckFreePages(i, count))
		{
			uint64_t j;
			for (j=0; j<count; j++)
			{
				PTe *pte = dmaGetPage(i+j);
				pte->present = 1;
				pte->framePhysAddr = physStart+j;
				pte->rw = 1;
			};
			
			refreshAddrSpace();
			return i;
		};
	};
	
	return 0;
};

int dmaCreateBuffer(DMABuffer *handle, size_t bufsize, int flags)
{
	uint64_t numFrames = bufsize / 0x1000;
	if (bufsize % 0x1000) numFrames++;
	
	spinlockAcquire(&dmaMemoryLock);
	uint64_t physStart = phmAllocFrameEx(numFrames, 0);
	
	if (physStart == 0)
	{
		spinlockRelease(&dmaMemoryLock);
		return DMA_ERR_NOMEM;
	};
	
	if (flags & DMA_32BIT)
	{
		uint64_t physEnd = physStart + numFrames;
		if (physEnd > 0x100000)
		{
			phmFreeFrameEx(physStart, numFrames);
			spinlockRelease(&dmaMemoryLock);
			return DMA_ERR_NOT32;
		};
	};
	
	uint64_t firstPage = dmaAllocPages(physStart, numFrames);
	if (firstPage == 0)
	{
		phmFreeFrameEx(physStart, numFrames);
		spinlockRelease(&dmaMemoryLock);
		return DMA_ERR_NOPAGES;
	};
	
	handle->firstFrame = physStart;
	handle->numFrames = numFrames;
	handle->firstPage = firstPage;
	spinlockRelease(&dmaMemoryLock);
	
	return 0;
};

void dmaReleaseBuffer(DMABuffer *handle)
{
	spinlockAcquire(&dmaMemoryLock);
	phmFreeFrameEx(handle->firstFrame, handle->numFrames);
	
	uint64_t i;
	for (i=0; i<handle->numFrames; i++)
	{
		PTe *pte = dmaGetPage(handle->firstPage+i);
		pte->present = 0;
		pte->framePhysAddr = 0;
	};
	
	refreshAddrSpace();
	
	// just to be safe
	handle->firstFrame = 0;
	handle->numFrames = 0;
	handle->firstPage = 0;
	
	spinlockRelease(&dmaMemoryLock);
};

uint64_t dmaGetPhys(DMABuffer *handle)
{
	return handle->firstFrame << 12;
};

void* dmaGetPtr(DMABuffer *handle)
{
	uint64_t addr = 0xFFFF838000000000 + (handle->firstPage << 12);
	return (void*) addr;
};
