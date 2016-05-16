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

#include <glidix/procmem.h>
#include <glidix/memory.h>
#include <glidix/console.h>
#include <glidix/physmem.h>
#include <glidix/isp.h>
#include <glidix/string.h>
#include <glidix/errno.h>
#include <glidix/sched.h>
#include <glidix/vfs.h>

static Spinlock mapLock;

FrameList *palloc_later(File *fp, int count, off_t fileOffset, size_t fileSize)
{
	FrameList *fl = (FrameList*) kmalloc(sizeof(FrameList));
	fl->fp = fp;
	if (fp != NULL) vfsDup(fp);
	fl->refcount = 1;
	fl->count = count;
	fl->frames = (uint64_t*) kmalloc(8*count);
	fl->fileOffset = fileOffset;
	fl->fileSize = fileSize;
	fl->flags = 0;
	fl->cowList = NULL;
	fl->on_destroy = NULL;
	memset(fl->frames, 0, 8*count);
	spinlockRelease(&fl->lock);
	return fl;
};

FrameList *palloc(int count)
{
	FrameList *fl = (FrameList*) kmalloc(sizeof(FrameList));
	fl->fp = NULL;
	fl->refcount = 1;
	fl->count = count;
	fl->frames = (uint64_t*) kmalloc(8*count);
	fl->fileOffset = -1;
	fl->fileSize = 0;
	fl->flags = 0;
	fl->cowList = NULL;
	fl->on_destroy = NULL;
	spinlockRelease(&fl->lock);

	int i;
	for (i=0; i<count; i++)
	{
		fl->frames[i] = phmAllocFrame();
	};

	return fl;
};

FrameList *pmap(uint64_t start, int count)
{
	FrameList *fl = (FrameList*) kmalloc(sizeof(FrameList));
	fl->fp = NULL;
	fl->refcount = 1;
	fl->count = count;
	fl->frames = (uint64_t*) kmalloc(8*count);
	fl->fileOffset = -1;
	fl->fileSize = 0;
	fl->flags = FL_SHARED;
	fl->cowList = NULL;
	fl->on_destroy = NULL;
	spinlockRelease(&fl->lock);
	
	int i;
	for (i=0; i<count; i++)
	{
		fl->frames[i] = start + i;
	};
	
	return fl;
};

int pupref(FrameList *fl)
{
	spinlockAcquire(&fl->lock);
	int ret = ++fl->refcount;
	if (fl->fp != NULL) vfsDup(fl->fp);
	spinlockRelease(&fl->lock);
	return ret;
};

uint64_t getFlagsRegister();
void pdownref(FrameList *fl)
{
	spinlockAcquire(&fl->lock);
	fl->refcount--;
	if (fl->refcount == 0)
	{
		if (fl->cowList != NULL)
		{
			spinlockAcquire(&fl->cowList->lock);
			int i;
			for (i=0; i<fl->count; i++)
			{
				if ((fl->frames[i] == 0) && (fl->cowList->frames[i] != 0))
				{
					if (fl->cowList->refcounts[i] > 0)
					{
						fl->cowList->refcounts[i]--;
						if (fl->cowList->refcounts[i] == 0) phmFreeFrame(fl->cowList->frames[i]);
						fl->cowList->pagesToGo--;
					};
				};
			};

			fl->cowList->users--;
			if (fl->cowList->users == 0)
			{
				kfree(fl->cowList->refcounts);
				kfree(fl->cowList->frames);
				kfree(fl->cowList);
			}
			else
			{
				spinlockRelease(&fl->cowList->lock);
			};
		}
		else
		{
			int i;
			for (i=0; i<fl->count; i++)
			{
				if (fl->frames[i] != 0) phmFreeFrame(fl->frames[i]);
			};
		};

		if (fl->on_destroy != NULL)
		{
			fl->on_destroy(fl->on_destroy_arg);
		};
		
		if (fl->fp != NULL)
		{
			vfsClose(fl->fp);
		};
		
		kfree(fl->frames);
		kfree(fl);
	};
	
	spinlockRelease(&fl->lock);
};

FrameList *pdup(FrameList *old)
{
	spinlockAcquire(&old->lock);
	if (old->flags & FL_SHARED)
	{
		old->refcount++;
		spinlockRelease(&old->lock);
		return old;
	};
	
	FrameList *fl = (FrameList*) kmalloc(sizeof(FrameList));
	fl->refcount = 1;
	fl->fp = old->fp;
	if (fl->fp != NULL) vfsDup(fl->fp);
	fl->count = old->count;
	fl->flags = old->flags;
	fl->frames = (uint64_t*) kmalloc(8*old->count);
	fl->fileOffset = old->fileOffset;
	fl->fileSize = old->fileSize;
	fl->on_destroy = NULL;
	spinlockRelease(&fl->lock);

	if (old->cowList == NULL)
	{
		COWList *list = (COWList*) kmalloc(sizeof(COWList));
		spinlockRelease(&list->lock);
		list->pagesToGo = 0;
		list->frames = (uint64_t*) kmalloc(8*old->count);
		list->refcounts = (uint64_t*) kmalloc(8*old->count);
		list->users = 1;

		int i;
		for (i=0; i<old->count; i++)
		{
			if (old->frames[i] != 0)
			{
				list->refcounts[i] = 1;
				list->frames[i] = old->frames[i];
				old->frames[i] = 0;
				list->pagesToGo++;
			}
			else
			{
				list->refcounts[i] = 0;
				list->frames[i] = 0;
			};
		};

		memset(old->frames, 0, 8*old->count);
		old->cowList = list;
	};

	spinlockAcquire(&old->cowList->lock);
	fl->cowList = old->cowList;
	fl->cowList->users++;

	int i;
	for (i=0; i<old->count; i++)
	{
		if (old->frames[i] != 0)
		{
			old->cowList->pagesToGo++;
			old->cowList->refcounts[i] = 1;
			old->cowList->frames[i] = old->frames[i];
			old->frames[i] = 0;
		};

		fl->frames[i] = 0;
		if (old->cowList->frames[i] != 0)
		{
			old->cowList->pagesToGo++;
			old->cowList->refcounts[i]++;
		};
	};

	spinlockRelease(&old->cowList->lock);
	spinlockRelease(&old->lock);

	return fl;
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
	pdpt->entries[511].present = 1;
	pdpt->entries[511].pdPhysAddr = pm->pdptPhysFrame;
	pdpt->entries[511].rw = 1;
	ispUnlock();

	spinlockRelease(&pm->lock);
	return pm;
};

static void pushCleanupFrame(ProcMem *pm, uint64_t frame)
{
	pm->framesToCleanUp = krealloc(pm->framesToCleanUp, 8*(pm->numFramesToCleanUp+1));
	pm->framesToCleanUp[pm->numFramesToCleanUp] = frame;
	pm->numFramesToCleanUp++;
};

static void splitPageIndex(uint64_t index, uint64_t *dirIndex, uint64_t *tableIndex, uint64_t *pageIndex)
{
	*pageIndex = (index & 0x1FF);
	index >>= 9;
	*tableIndex = (index & 0x1FF);
	index >>= 9;
	*dirIndex = (index & 0x1FF);
};

/**
 * Return a page table entry for the specified page index. If MEM_MAKE is passed in flags, then
 * the page directory and page table for storing this entry will be created if they don't exist
 * (the returned page will still be marked non-present), otherwise NULL is returned if the page
 * table does not exist. 'sel' shall be MEM_CURRENT for pml4[0] and MEM_OTHER for pml4[1].
 */
static PTe *getPage(ProcMem *pm, MemorySelector sel, uint64_t index, int flags)
{
	uint64_t dirIndex, tableIndex, pageIndex;
	splitPageIndex(index, &dirIndex, &tableIndex, &pageIndex);
	uint64_t base = 0x8000000000 * sel;

	int makeDir=0, makeTable=0;

	PDPT *pdpt = (PDPT*) (base + 0x7FFFFFF000);
	if (!pdpt->entries[dirIndex].present)
	{
		if (flags & MEM_MAKE)
		{
			pdpt->entries[dirIndex].present = 1;
			uint64_t frame = phmAllocFrame();
			pdpt->entries[dirIndex].pdPhysAddr = frame;
			pdpt->entries[dirIndex].user = 1;
			pdpt->entries[dirIndex].rw = 1;
			pushCleanupFrame(pm, frame);
			refreshAddrSpace();
			makeDir = 1;
		}
		else
		{
			return NULL;
		};
	};

	PD *pd = (PD*) (base + 0x7FFFE00000 + dirIndex * 0x1000);
	if (makeDir) memset(pd, 0, 0x1000);
	if (!pd->entries[tableIndex].present)
	{
		if (flags & MEM_MAKE)
		{
			pd->entries[tableIndex].present = 1;
			uint64_t frame = phmAllocFrame();
			pd->entries[tableIndex].ptPhysAddr = frame;
			pd->entries[tableIndex].user = 1;
			pd->entries[tableIndex].rw = 1;
			pushCleanupFrame(pm, frame);
			refreshAddrSpace();
			makeTable = 1;
		}
		else
		{
			return NULL;
		};
	};

	PT *pt = (PT*) (base + 0x7FC0000000 + dirIndex * 0x200000 + tableIndex * 0x1000);
	if (makeTable) memset(pt, 0, 0x1000);
	return &pt->entries[pageIndex];
};

int isPageMapped(uint64_t addr, int writeable)
{
	PTe *pte = getPage(NULL, MEM_CURRENT, addr>>12, 0);
	if (pte == NULL) return 0;
	if (!pte->present) return 0;
	if (!pte->user) return 0;
	
	if (writeable)
	{
		if (!pte->rw) return 0;
	};
	
	return 1;
};

int AddSegment(ProcMem *pm, uint64_t start, FrameList *frames, int flags)
{
	uint64_t ignore;
	return AddSegmentEx(pm, start, frames, flags, &ignore);
};

int AddSegmentEx(ProcMem *pm, uint64_t start, FrameList *frames, int flags, uint64_t *realAddr)
{
	if (start >= 0x7FC0000) return MEM_SEGMENT_COLLISION;

	spinlockAcquire(&pm->lock);
	spinlockAcquire(&frames->lock);

	// ensure that this segment wouldn't clash with another segment
	Segment *scan = pm->firstSegment;
	//Segment *lastSegment = NULL;
	uint64_t count = (uint64_t) frames->count;
	uint64_t endPage = start + count - 1;
	while (scan != NULL)
	{
		// if we're allocating instead of using a fixed location
		if (start == 0)
		{
			if (scan->next == NULL)
			{
				start = scan->start + scan->fl->count;
				break;
			}
			else
			{
				uint64_t wouldStart = scan->start + scan->fl->count;
				if ((wouldStart + count) < scan->next->start)
				{
					start = wouldStart;
					break;
				};
			};
			
			scan = scan->next;
			continue;
		};
		
		if ((start >= scan->start) && (start < (scan->start+scan->fl->count)))
		{
			spinlockRelease(&frames->lock);
			spinlockRelease(&pm->lock);
			return MEM_SEGMENT_COLLISION;
		};
		if (((start+count) < (scan->start+scan->fl->count)) && ((start+count) > scan->start))
		{
			spinlockRelease(&frames->lock);
			spinlockRelease(&pm->lock);
			return MEM_SEGMENT_COLLISION;
		};
		
		int beforeNext = 1;
		if (scan->next != NULL)
		{
			beforeNext = (endPage < scan->next->start);
		};
		
		if ((start >= (scan->start+scan->fl->count)) && (beforeNext))
		{
			// we place ourselves after this segment to maintain sorted segments
			break;
		};
		
		if (endPage < scan->start)
		{
			// insert it before all segments
			scan = NULL;
			break;
		};
		
		//lastSegment = scan;
		scan = scan->next;
	};
	
	// if there are no segments yet, start mapping at 0x1000
	if (start == 0)
	{
		start = 1;
	};
	
	*realAddr = (start << 12);

	// create the segment
	Segment *segment = (Segment*) kmalloc(sizeof(Segment));
	segment->start = start;
	segment->fl = frames;
	segment->flags = flags;
	if (scan == NULL)
	{
		segment->next = pm->firstSegment;
		pm->firstSegment = segment;
	}
	else
	{
		segment->next = scan->next;
		scan->next = segment;
	};

	Thread *ct = getCurrentThread();
	MemorySelector selector = MEM_CURRENT;
	if (pm != ct->pm)
	{
		selector = MEM_OTHER;
	};

	if (selector == MEM_OTHER)
	{
		spinlockAcquire(&mapLock);
		PML4 *pml4 = getPML4();
		pml4->entries[1].present = 1;
		pml4->entries[1].pdptPhysAddr = pm->pdptPhysFrame;
		pml4->entries[1].rw = 1;
		refreshAddrSpace();
	};

	uint64_t i;
	for (i=0; i<count; i++)
	{
		PTe *pte = getPage(pm, selector, start+i, MEM_MAKE);
		if ((frames->cowList != NULL) && (frames->frames[i] == 0))
		{
			if (frames->cowList->frames[i] != 0)
			{
				pte->present = 1;
				pte->rw = 0;
				pte->user = 1;
				pte->framePhysAddr = frames->cowList->frames[i];
			}
			else
			{
				// load-on-demand page
				pte->present = 0;
				pte->rw = !!(flags & PROT_WRITE);
				pte->user = 1;
				pte->framePhysAddr = 0;
			};
		}
		else
		{
			pte->present = !!(frames->frames[i] != 0);
			pte->rw = !!(flags & PROT_WRITE);
			pte->user = 1;
			pte->framePhysAddr = frames->frames[i];
		};
	};

	frames->refcount++;

	if (selector == MEM_OTHER)
	{
		PML4 *pml4 = getPML4();
		memset(&pml4->entries[1], 0, 8);
		spinlockRelease(&mapLock);
		refreshAddrSpace();
	};

	spinlockRelease(&frames->lock);
	spinlockRelease(&pm->lock);
	return 0;
};

int DeleteSegment(ProcMem *pm, uint64_t start)
{
	if (pm != getCurrentThread()->pm) panic("DeleteSegment can only be called on the current memory\n");
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
	for (i=0; i<seg->fl->count; i++)
	{
		PTe *pte = getPage(NULL, MEM_CURRENT, start+i, 0);
		if (pte != NULL)
		{
			if (pte->present)
			{
				memset(pte, 0, 8);
			};
		};
	};

	// deallocate the segment data
	pdownref(seg->fl);
	kfree(seg);

	spinlockRelease(&pm->lock);
	return 0;
};

extern uint64_t getFlagsRegister();

void SetProcessMemory(ProcMem *pm)
{
	int shouldEnable = getFlagsRegister() & (1 << 9);
	PML4 *pml4 = getPML4();
	cli();
	pml4->entries[0].present = 1;
	pml4->entries[0].rw = 1;
	pml4->entries[0].user = 1;
	pml4->entries[0].pdptPhysAddr = pm->pdptPhysFrame;
	refreshAddrSpace();
	if (shouldEnable) sti();
};

static void DeleteProcessMemory(ProcMem *pm)
{
	// don't bother unmapping page tables because they're gonna be removed anyway.
	// just downref all the frame lists.
	Segment *scan = pm->firstSegment;
	while (scan != NULL)
	{
		Segment *next = scan->next;
		pdownref(scan->fl);
		kfree(scan);
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
	if (pm != getCurrentThread()->pm)
	{
		panic("please only call DuplicateProcessMemory on the current process' memory.");
	};

	ProcMem *new = CreateProcessMemory();

	Segment *seg = pm->firstSegment;
	while (seg != NULL)
	{
		/**
		 * Mark the pages read-only, for copy-on-write, as long as the framelist is not shared.
		 */
		if ((seg->fl->flags & FL_SHARED) == 0)
		{
			int i;
			for (i=0; i<seg->fl->count; i++)
			{
				PTe *pte = getPage(pm, MEM_CURRENT, seg->start + i, 0);
				if (pte != NULL)			// pte might be NULL for load-on-demand pages
				{
					if (pte->present)		// may be non-present if its load-on-demand and unloaded
					{
						pte->rw = 0;
					};
				};
			};
		};

		FrameList *newList = pdup(seg->fl); /*pdup_noclone(seg->fl);*/
		AddSegment(new, seg->start, newList, seg->flags);
		pdownref(newList);
		seg = seg->next;
	};

	refreshAddrSpace();
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
		return;
	};
	spinlockRelease(&pm->lock);
};

int tryCopyOnWrite(uint64_t addr)
{
	// don't lock the procmem object; it's already locked
	ProcMem *pm = getCurrentThread()->pm;

	uint64_t pageIndex = addr / 0x1000;
	Segment *seg = pm->firstSegment;

	while (seg != NULL)
	{
		if (seg->start <= pageIndex)
		{
			uint64_t relidx = pageIndex - seg->start;
			spinlockAcquire(&seg->fl->lock);
			if (relidx < seg->fl->count)
			{
				if ((seg->fl->frames[relidx] == 0) && (seg->flags & PROT_WRITE))
				{
					if (seg->fl->cowList != NULL)
					{
						spinlockAcquire(&seg->fl->cowList->lock);
						if (seg->fl->cowList->frames[relidx] != 0)
						{
							// this means we have something to copy-on-write.
							if (seg->fl->cowList->refcounts[relidx] == 1)
							{
								// this page belongs solely to us.
								seg->fl->frames[relidx] = seg->fl->cowList->frames[relidx];
								seg->fl->cowList->refcounts[relidx] = 0;
								seg->fl->cowList->pagesToGo--;
								spinlockRelease(&seg->fl->cowList->lock);

								PTe *pte = getPage(NULL, MEM_CURRENT, pageIndex, 0);
								pte->rw = 1;
								refreshAddrSpace();
								spinlockRelease(&seg->fl->lock);
								return 0;
							};

							// someone else owns this page too, we must copy it.
							uint64_t newFrame = phmAllocFrame();
							ispLock();
							ispSetFrame(newFrame);
							memcpy(ispGetPointer(), (void*)(0x1000 * pageIndex), 0x1000);
							ispUnlock();

							PTe *pte = getPage(NULL, MEM_CURRENT, pageIndex, 0);
							if (pte == NULL) panic("pte == NULL");
							if (!pte->user) panic("!pte->user");
							pte->framePhysAddr = newFrame;
							pte->rw = 1;
							refreshAddrSpace();
							seg->fl->frames[relidx] = newFrame;

							seg->fl->cowList->refcounts[relidx]--;
							seg->fl->cowList->pagesToGo--;

							spinlockRelease(&seg->fl->cowList->lock);
							spinlockRelease(&seg->fl->lock);
							return 0;
						};
						spinlockRelease(&seg->fl->cowList->lock);
					};
				};
			};

			spinlockRelease(&seg->fl->lock);
		};

		seg = seg->next;
	};

	return -1;
};

int tryLoadOnDemand(uint64_t addr)
{
	// do not lock the procmem object; it's already locked
	ProcMem *pm = getCurrentThread()->pm;

	uint64_t pageIndex = addr / 0x1000;
	Segment *seg = pm->firstSegment;

	char pagebuf[0x1000];
	
	while (seg != NULL)
	{
		if (seg->start <= pageIndex)
		{
			uint64_t relidx = pageIndex - seg->start;
			spinlockAcquire(&seg->fl->lock);
			if (relidx < seg->fl->count)
			{
				if (seg->fl->frames[relidx] == 0)
				{
					off_t offset = (seg->fl->fileOffset + (relidx * 0x1000)) & ~0xFFF;
					size_t size = 0x1000;
					while (((offset+size) > (seg->fl->fileOffset+seg->fl->fileSize)) && (size != 0))
					{
						size--;
					};
					memset(pagebuf, 0, 0x1000);
					File *fp = seg->fl->fp;
					if (fp != NULL)
					{
						fp->seek(fp, offset, SEEK_SET);
						vfsRead(fp, pagebuf, size);
					};
					
					PTe *pte = getPage(pm, MEM_CURRENT, pageIndex, 0);
					uint64_t newFrame = phmAllocFrame();
					ispLock();
					ispSetFrame(newFrame);
					memcpy(ispGetPointer(), pagebuf, 0x1000);
					ispUnlock();
					seg->fl->frames[relidx] = newFrame;
					pte->framePhysAddr = newFrame;
					pte->present = 1;
					refreshAddrSpace();

					spinlockRelease(&seg->fl->lock);
					return 0;
				};
			};
			spinlockRelease(&seg->fl->lock);
		};

		seg = seg->next;
	};

	return -1;
};

void dumpProcessMemory(ProcMem *pm, uint64_t checkAddr)
{
	static const char *sizeUnits[5] = {
		"B", "KB", "MB", "GB", "TB"
	};

	uint64_t frameInList=0, frameInCOW=0, cowRefcount=0;
	kprintf(" BASE\t\t\tEND\t\t\tSIZE\tFLAGS\n");
	Segment *seg;
	for (seg=pm->firstSegment; seg!=NULL; seg=seg->next)
	{
		uint64_t base = seg->start * 0x1000;
		uint64_t size = seg->fl->count * 0x1000;
		uint64_t end = base + size - 1;

		int displaySize = (int) size;
		int sizeIndex = 0;

		while ((sizeIndex < 5) && (displaySize > 1024))
		{
			sizeIndex++;
			displaySize /= 1024;
		};

		char arrow = ' ';
		if ((base <= checkAddr) && (end > checkAddr))
		{
			arrow = '>';
			uint64_t index = (checkAddr - base)/0x1000;
			frameInList = seg->fl->frames[index];
			if (seg->fl->cowList != NULL)
			{
				frameInCOW = seg->fl->cowList->frames[index];
				cowRefcount = seg->fl->cowList->refcounts[index];
			};
		};

		kprintf("%c%a\t%a\t%d%s\t", arrow, base, end, displaySize, sizeUnits[sizeIndex]);
		PRINTFLAG(seg->flags & PROT_READ, 'R');
		PRINTFLAG(seg->flags & PROT_WRITE, 'W');
		PRINTFLAG(seg->flags & PROT_EXEC, 'X');
		PRINTFLAG(seg->fl->flags & FL_SHARED, 'S');
		kprintf("\n");
	};
	kprintf("Virtual addr %a is frame %a and cow frame %a (refcount %d)\n", checkAddr, frameInList, frameInCOW, cowRefcount);
};

int canAccessPage(ProcMem *pm, uint64_t pageindex, int perms)
{
	if (pm == NULL) return 0;

	spinlockAcquire(&pm->lock);
	Segment *seg;
	
	for (seg=pm->firstSegment; seg!=NULL; seg=seg->next)
	{
		uint64_t endframe = seg->start + seg->fl->count;
		if ((pageindex >= seg->start) && (pageindex < endframe))
		{
			int status = 1;
			if ((seg->flags & perms) != perms)
			{
				// not all perms are set
				status = 0;
			};
			
			spinlockRelease(&pm->lock);
			return status;
		};
	};
	
	// the memory is not mapped
	spinlockRelease(&pm->lock);
	return 0;
};

int mprotect(uint64_t addr, uint64_t len, int prot)
{
	if ((addr < 0x1000) || ((addr+len) > 0x7FC0000000))
	{
		getCurrentThread()->therrno = ENOMEM;
		return -1;
	};

	if ((addr % 0x1000) != 0)
	{
		getCurrentThread()->therrno = EINVAL;
		return -1;
	};

	if ((prot & PROT_ALL) != prot)
	{
		getCurrentThread()->therrno = EACCES;
		return -1;
	};

	uint64_t start = addr / 0x1000;
	uint64_t count = len / 0x1000;
	if (len % 0x1000) count++;

	ProcMem *pm = getCurrentThread()->pm;
	if (prot & PROT_ALLOC)
	{
		int flags = prot & (PROT_READ | PROT_WRITE | PROT_EXEC);
		FrameList *fl = palloc(count);
		if (AddSegment(pm, start, fl, flags) != 0)
		{
			pdownref(fl);
			getCurrentThread()->therrno = ENOMEM;
			return -1;
		};
		pdownref(fl);
	}
	else
	{
		// NOTE: for now, Glidix will ignore requests to change memory protection.
		// we may implement it later. Discussion needed.
		if (prot == 0)
		{
			if (DeleteSegment(pm, start) != 0)
			{
				getCurrentThread()->therrno = ENOMEM;
				return -1;
			};
		}
		else
		{
			getCurrentThread()->therrno = ENOMEM;
			return -1;
		};
	};

	SetProcessMemory(pm);
	
	if (prot & PROT_ALLOC)
	{
		// zero out allocated memory!
		memset((void*)(start * 0x1000), 0, count * 0x1000);
	};
	
	return 0;
};
