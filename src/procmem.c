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

#include <glidix/procmem.h>
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/physmem.h>
#include <glidix/isp.h>
#include <glidix/string.h>

FrameList *palloc(int count)
{
	FrameList *fl = (FrameList*) kmalloc(sizeof(FrameList));
	fl->refcount = 1;
	fl->count = count;
	fl->frames = (uint64_t*) kmalloc(8*count);

	int i;
	for (i=0; i<count; i++)
	{
		fl->frames[i] = phmAllocFrame();
	};

	return fl;
};

void pupref(FrameList *fl)
{
	fl->refcount++;
};

void pdownref(FrameList *fl)
{
	fl->refcount--;
	if (fl->refcount == 0)
	{
		int i;
		for (i=0; i<fl->count; i++)
		{
			phmFreeFrame(fl->frames[i]);
		};

		kfree(fl->frames);
		kfree(fl);
	};
};

FrameList *pdup(FrameList *old)
{
	FrameList *new = palloc(old->count);

	// we will use this page buffer to copy data between the old frames and the new
	// frames.
	void *pagebuf = kmalloc(0x1000);

	int i;
	ispLock();
	for (i=0; i<old->count; i++)
	{
		ispSetFrame(old->frames[i]);
		memcpy(pagebuf, ispGetPointer(), 0x1000);
		ispSetFrame(new->frames[i]);
		memcpy(ispGetPointer(), pagebuf, 0x1000);
	};
	ispUnlock();

	kfree(pagebuf);
	return new;
};

ProcMem *CreateProcessMemory()
{
	ProcMem *pm = (ProcMem*) kmalloc(sizeof(ProcMem));
	pm->refcount = 1;
	pm->pdptPhysFrame = phmAllocFrame();
	pm->firstSegment = NULL;
	pm->framesToCleanUp = NULL;
	pm->numFramesToCleanUp = 0;

	ispLock();
	ispSetFrame(pm->pdptPhysFrame);
	PDPT *pdpt = (PDPT*) ispGetPointer();
	memset(pdpt, 0, sizeof(PDPT));
	ispUnlock();

	spinlockRelease(&pm->lock);
	return pm;
};

static void splitPageIndex(uint64_t index, uint64_t *dirIndex, uint64_t *tableIndex, uint64_t *pageIndex)
{
	*pageIndex = (index & 0x1FF);
	index >>= 9;
	*tableIndex = (index & 0x1FF);
	index >>= 9;
	*dirIndex = (index & 0x1FF);
};

static void pushCleanupFrame(ProcMem *pm, uint64_t frame)
{
	pm->framesToCleanUp = krealloc(pm->framesToCleanUp, 8*(pm->numFramesToCleanUp+1));
	pm->framesToCleanUp[pm->numFramesToCleanUp] = frame;
	pm->numFramesToCleanUp++;
};

// call this function only when ISP is locked.
static void findPageTable(ProcMem *pm, uint64_t index, uint64_t *ptPhysFramePTR, uint64_t *pageIndexPTR)
{
	ispSetFrame(pm->pdptPhysFrame);

	uint64_t dirIndex, tableIndex, pageIndex;
	splitPageIndex(index, &dirIndex, &tableIndex, &pageIndex);

	PDPT *pdpt = (PDPT*) ispGetPointer();
	if (!pdpt->entries[dirIndex].present)
	{
		pdpt->entries[dirIndex].present = 1;
		pdpt->entries[dirIndex].rw = 1;
		pdpt->entries[dirIndex].user = 1;
		pdpt->entries[dirIndex].pdPhysAddr = phmAllocFrame();
		pushCleanupFrame(pm, pdpt->entries[dirIndex].pdPhysAddr);
		ispSetFrame(pdpt->entries[dirIndex].pdPhysAddr);
		memset(ispGetPointer(), 0, 0x1000);
	}
	else
	{
		ispSetFrame(pdpt->entries[dirIndex].pdPhysAddr);
	};

	PD *pd = (PD*) ispGetPointer();
	if (!pd->entries[tableIndex].present)
	{
		pd->entries[tableIndex].present = 1;
		pd->entries[tableIndex].rw = 1;
		pd->entries[tableIndex].user = 1;
		pd->entries[tableIndex].ptPhysAddr = phmAllocFrame();
		*ptPhysFramePTR = pd->entries[tableIndex].ptPhysAddr;
		pushCleanupFrame(pm, *ptPhysFramePTR);
		ispSetFrame(*ptPhysFramePTR);
		memset(ispGetPointer(), 0, 0x1000);
	}
	else
	{
		*ptPhysFramePTR = pd->entries[tableIndex].ptPhysAddr;
	};

	*pageIndexPTR = pageIndex;
};

int AddSegment(ProcMem *pm, uint64_t start, FrameList *frames, int flags)
{
	// the first page is not allowed to be mapped
	if (start == 0) return MEM_SEGMENT_COLLISION;

	spinlockAcquire(&pm->lock);

	// ensure that this segment wouldn't clash with another segment
	Segment *scan = pm->firstSegment;
	Segment *lastSegment = NULL;
	uint64_t count = (uint64_t) frames->count;
	while (scan != NULL)
	{
		if ((start >= scan->start) && (start < (scan->start+scan->fl->count)))
		{
			return MEM_SEGMENT_COLLISION;
		};
		if (((start+count) < (scan->start+scan->fl->count)) && ((start+count) > scan->start))
		{
			return MEM_SEGMENT_COLLISION;
		};
		lastSegment = scan;
		scan = scan->next;
	};

	// create the segment
	Segment *segment = (Segment*) kmalloc(sizeof(Segment));
	segment->start = start;
	segment->fl = frames;
	segment->flags = flags;
	segment->next = NULL;

	if (lastSegment == NULL)
	{
		pm->firstSegment = segment;
	}
	else
	{
		lastSegment->next = segment;
	};

	// map the memory
	uint64_t i;
	ispLock();
	for (i=0; i<count; i++)
	{
		uint64_t ptPhysFrame, pageIndex;
		findPageTable(pm, start+i, &ptPhysFrame, &pageIndex);
		ispSetFrame(ptPhysFrame);
		PT *pt = (PT*) ispGetPointer();

		if (pt->entries[pageIndex].present)
		{
			memset(&pt->entries[pageIndex], 0, 8);
		};

		pt->entries[pageIndex].present = 1;
		pt->entries[pageIndex].rw = !!(flags & PROT_WRITE);
		pt->entries[pageIndex].user = 1;
		pt->entries[pageIndex].framePhysAddr = frames->frames[i];
	};
	frames->refcount++;
	ispUnlock();

	spinlockRelease(&pm->lock);
	return 0;
};

int DeleteSegment(ProcMem *pm, uint64_t start)
{
	spinlockAcquire(&pm->lock);

	Segment *seg = pm->firstSegment;
	if (seg == NULL)
	{
		spinlockRelease(&pm->lock);
		return MEM_SEGMENT_INVALID;
	};

	Segment *sprev = NULL;
	while (seg->start != start)
	{
		if (seg->next == NULL)
		{
			spinlockRelease(&pm->lock);
			return MEM_SEGMENT_INVALID;
		};

		sprev = seg;
		seg = seg->next;
	};

	// remove the segment from the list
	if (sprev != NULL) sprev->next = seg->next;
	else pm->firstSegment = NULL;
	seg->next = NULL;				// just in case

	// unmap all the pages
	uint64_t i;
	ispLock();
	for (i=0; i<seg->fl->count; i++)
	{
		uint64_t ptPhysFrame, pageIndex;
		findPageTable(pm, start+i, &ptPhysFrame, &pageIndex);
		ispSetFrame(ptPhysFrame);
		PT *pt = (PT*) ispGetPointer();

		if (pt->entries[pageIndex].present)
		{
			// wipe the page mapping
			memset(&pt->entries[pageIndex], 0, 8);
		};
	};
	ispUnlock();

	// deallocate the segment data
	pdownref(seg->fl);
	kfree(seg);

	spinlockRelease(&pm->lock);
	return 0;
};

void SetProcessMemory(ProcMem *pm)
{
	PML4 *pml4 = getPML4();
	pml4->entries[0].present = 1;
	pml4->entries[0].rw = 1;
	pml4->entries[0].user = 1;
	pml4->entries[0].pdptPhysAddr = pm->pdptPhysFrame;
};

static void DeleteProcessMemory(ProcMem *pm)
{
	// this procedure is somewhat dirty; scans the list of segments and calls DeleteSegment()
	// which scans the list again... we should probably separate that.
	Segment *scan = pm->firstSegment;
	while (scan != NULL)
	{
		Segment *next = scan->next;
		DeleteSegment(pm, scan->start);
		scan = next;
	};

	// clean up all the frames (those contain parts of the page structure).
	int i;
	for (i=0; i<pm->numFramesToCleanUp; i++)
	{
		phmFreeFrame(pm->framesToCleanUp[i]);
	};
	kfree(pm->framesToCleanUp);

	phmFreeFrame(pm->pdptPhysFrame);
	kfree(pm);
};

ProcMem *DuplicateProcessMemory(ProcMem *pm)
{
	ProcMem *new = CreateProcessMemory();

	Segment *seg = pm->firstSegment;
	while (seg != NULL)
	{
		FrameList *newList = pdup(seg->fl);
		AddSegment(pm, seg->start, newList, seg->flags);
		pdownref(newList);
		seg = seg->next;
	};

	return new;
};

void UprefProcessMemory(ProcMem *pm)
{
	spinlockAcquire(&pm->lock);
	pm->refcount++;
	spinlockRelease(&pm->lock);
};

void DownrefProcessMemory(ProcMem *pm)
{
	spinlockAcquire(&pm->lock);
	pm->refcount--;
	if (pm->refcount == 0)
	{
		spinlockRelease(&pm->lock);
		DeleteProcessMemory(pm);
		return;				// not need to unlock because nobody will try locking ever again.
	};
	spinlockRelease(&pm->lock);
};
