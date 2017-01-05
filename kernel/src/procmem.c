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
#include <glidix/sched.h>
#include <glidix/physmem.h>
#include <glidix/pagetab.h>
#include <glidix/errno.h>
#include <glidix/vfs.h>

int vmNew()
{
	Thread *ct = getCurrentThread();
	if (ct->pm != NULL)
	{
		if (__sync_add_and_fetch(&ct->pm->refcount, -1) == 0)
		{
			// TODO: delete the address space and set to NULL and stuff
		};
	};
	
	ProcMem *pm = NEW(ProcMem);
	if (pm == NULL)
	{
		return -1;
	};
	
	semInit(&pm->lock);
	
	// create the initial blank segment
	Segment *seg = NEW(Segment);
	if (seg == NULL)
	{
		kfree(pm);
		return -1;
	};
	
	seg->prev = seg->next = NULL;
	seg->numPages = 0x8000000;
	seg->ft = NULL;
	seg->flags = 0;
	seg->prot = 0;
	
	pm->segs = seg;
	pm->refcount = 1;
	pm->phys = phmAllocZero();
	
	ct->pm = pm;
	
	PML4 *pml4 = getPML4();
	pml4->entries[0].framePhysAddr = pm->phys;
	pml4->entries[0].user = 1;
	pml4->entries[0].rw = 1;
	pml4->entries[0].present = 1;
	
	refreshAddrSpace();
	return 0;
};

uint64_t vmMap(uint64_t addr, size_t len, int prot, int flags, File *fp, off_t off)
{
	if (fp == NULL)
	{
		if ((flags & MAP_ANON) == 0)
		{
			// not anonymous, yet no file was specified
			return EBADF;
		};
	}
	else if (fp->tree == NULL)
	{
		// this file cannot be mapped
		return ENODEV;
	};

	int mappingType = flags & (MAP_PRIVATE | MAP_SHARED);
	if ((mappingType != MAP_PRIVATE) && (mappingType != MAP_SHARED))
	{
		// neither private nor shared, or both
		return EINVAL;
	};
		
	if ((addr != 0) || (flags & MAP_FIXED))
	{
		if (addr < 0x200000)
		{
			return EINVAL;
		};
		
		if ((addr+len) > 0x8000000000)
		{
			return EINVAL;
		};
	};
	
	if (off & 0xFFF)
	{
		return EINVAL;
	};
	
	uint64_t numPages = (len + 0xFFF) >> 12;
	if (numPages == 0)
	{
		return EINVAL;
	};
	
	Thread *creator = NULL;
	if (flags & MAP_THREAD)
	{
		creator = getCurrentThread();
	};
	
	if ((addr == 0) && ((flags & MAP_FIXED) == 0))
	{
		// find an available mapping at the highest possible address
		ProcMem *pm = getCurrentThread()->pm;
		semWait(&pm->lock);
		
		// get the last segment
		Segment *seg = pm->segs;
		while (seg->next != NULL) seg = seg->next;
		
		// find the furthest FREE segment that can storethe required
		// number of pages, or return ENOMEM.
		uint64_t pos = 0x8000000000 - (seg->numPages << 12);
		while ((seg->flags != 0) || (seg->numPages < numPages))
		{
			if (seg->prev == NULL)
			{
				semSignal(&pm->lock);
				return ENOMEM;
			};
			
			seg = seg->prev;
			pos -= seg->numPages << 12;
		};
		
		if (seg->numPages == numPages)
		{
			if (pos < ADDR_MIN)
			{
				semSignal(&pm->lock);
				return ENOMEM;
			};
			
			// just modify the existing segment
			seg->ft = fp->tree(fp);
			seg->offset = off;
			seg->creator = creator;
			seg->flags = flags;
			seg->prot = prot;
		}
		else
		{
			pos += ((seg->numPages - numPages) << 12);
			if (pos < ADDR_MIN)
			{
				semSignal(&pm->lock);
				return ENOMEM;
			};
			
			// split segment into 2
			Segment *newSeg = NEW(Segment);
			newSeg->prev = seg;
			newSeg->next = seg->next;
			
			if (seg->next != NULL)
			{
				seg->next->prev = newSeg;
			};
			
			seg->next = newSeg;
			
			newSeg->numPages = numPages;
			newSeg->ft = fp->tree(fp);
			newSeg->offset = off;
			newSeg->creator = creator;
			newSeg->flags = flags;
			newSeg->prot = prot;
			
			seg->numPages -= numPages;
		};
		
		semSignal(&pm->lock);
		return pos;
	}
	else
	{
		if (flags & MAP_FIXED)
		{
			if (addr & 0xFFF)
			{
				return EINVAL;
			};
		};
		
		addr &= ~0xFFF;
		
		// first find the segment which contains 'addr'.
		ProcMem *pm = getCurrentThread()->pm;
		semWait(&pm->lock);
		
		uint64_t pos = 0;
		Segment *seg = pm->segs;
		
		while ((pos+(seg->numPages<<12)) >= addr)
		{
			if (seg->next == NULL)
			{
				semSignal(&pm->lock);
				return ENOMEM;
			};
			
			pos += (seg->numPages << 12);
			seg = seg->next;
		};
		
		if ((pos == addr) && (seg->numPages == numPages)
		{
			// we can just modify this segment
			if (seg->ft != NULL)
			{
				ftDown(seg->ft);
			};
			
			if (seg->flags != 0)
			{
				unmapArea(pos, numPages << 12);
			};
			
			seg->ft = fp->tree(fp);
			seg->offset = off;
			seg->creator = creator;
			seg->flags = flags;
			seg->prot = prot;
		}
		else
		{
			// if we are not exactly at the start of this segment, we must
			// truncate it and add a new segment.
			if (pos < addr)
			{
				uint64_t offsetPages = (addr - pos) >> 12;
				
				Segment *newSeg = NEW(Segment);
				memcpy(newSeg, seg, sizeof(Segment));
				if (newSeg->ft != NULL) ftUp(newSeg->ft);
				
				newSeg->numPages -= offsetPages;
				seg->numPages = offsetPges;
				newSeg->offset += (offsetPages << 12);
				
				newSeg->next = seg->next;
				if (newSeg->next != NULL) newSeg->next->prev = newSeg;
				
				newSeg->prev = seg;
				seg->next = newSeg;
				
				seg = newSeg;
			};
			
			// if it is too big, rip off the end
			if (seg->numPages > numPages)
			{
				Segment *newSeg = NEW(Segment);
				memcpy(newSeg, seg, sizeof(Segment));
				if (newSeg->ft != NULL) ftUp(newSeg->ft);
				
				newSeg->numPages = seg->numPages - numPages;
				newSeg->offset += (numPages << 12);
				
				seg->numPages = numPages;
				
				newSeg->next = seg->next;
				if (newSeg->next != NULL) newSeg->next->prev = newSeg;
				
				newSeg->prev = seg;
				seg->next = newSeg;
			};
			
			// if too small, consume further segments until necessary space is found
			while (seg->numPages < numPages)
			{
				assert(seg->next != NULL);
				
				uint64_t pagesNeeded = numPages - seg->numPages;
				if (seg->next->numPages <= pagesNeeded)
				{
					if (seg->next->flags != 0)
					{
						unmapArea(addr + (seg->numPages << 12), seg->numPages << 12);
					};
					
					if (seg->next->ft != NULL) ftDown(seg->next->ft);
					
					seg->numPages += seg->next->numPages;
					
					Segment *next = seg->next;
					seg->next = next->next;
					if (seg->next != NULL) seg->next->prev = seg;
					
					kfree(next);
				}
				else
				{
					seg->next->offset += pagesNeeded << 12;
					seg->next->numPages -= pagesNeeded;
					seg->numPages += pagesNeeded;
				};
			};
			
			// fill in details now
			if (seg->flags != 0)
			{
				unmapArea(addr, numPages << 12);
			};
			
			if (seg->ft != NULL) ftDown(seg->ft);
			seg->ft = fp->tree(fp);
			seg->offset = off;
			seg->creator = creator;
			seg->flags = flags;
			seg->prot = prot;
		};
		
		semSignal(&pm->lock);
		return addr;
	};
};
