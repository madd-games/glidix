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

#include <glidix/thread/procmem.h>
#include <glidix/thread/sched.h>
#include <glidix/hw/physmem.h>
#include <glidix/hw/pagetab.h>
#include <glidix/util/errno.h>
#include <glidix/fs/vfs.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>
#include <glidix/thread/pageinfo.h>
#include <glidix/display/console.h>
#include <glidix/util/catch.h>

static PTe *getPage(uint64_t addr, int make)
{
	// page-align
	addr &= ~0xFFF;
	
	PDPTe *pdpte = (PDPTe*) (((addr >> 27) | 0xffffffffffe00000UL) & ~0x7);
	if (!pdpte->present)
	{
		if (make)
		{
			pdpte->pdPhysAddr = phmAllocZeroFrame();
			pdpte->user = 1;
			pdpte->rw = 1;
			pdpte->present = 1;
			refreshAddrSpace();
		}
		else
		{
			return NULL;
		};
	};
	
	PDe *pde = (PDe*) (((addr >> 18) | 0xffffffffc0000000UL) & ~0x7);
	if (!pde->present)
	{
		if (make)
		{
			pde->ptPhysAddr = phmAllocZeroFrame();
			pde->user = 1;
			pde->rw = 1;
			pde->present = 1;
			refreshAddrSpace();
		}
		else
		{
			return NULL;
		};
	};
	
	return (PTe*) (((addr >> 9) | 0xffffff8000000000UL) & ~0x7);
};

static void invalidatePage(uint64_t addr)
{
	// TODO: inter-CPU TLB flush
	invlpg((void*)addr);
};

static void unmapArea(uint64_t base, uint64_t size)
{
	uint64_t pos;
	for (pos=base; pos<(base+size); pos+=0x1000)
	{
		PTe *pte = getPage(pos, 0);
		if (pte != NULL)
		{
			if (pte->gx_loaded)
			{
				pte->present = 0;
				pte->gx_loaded = 0;
				invalidatePage(pos);
				if (pte->accessed) piMarkAccessed(pte->framePhysAddr);
				if (pte->dirty) piMarkDirty(pte->framePhysAddr);
				piDecref(pte->framePhysAddr);
				*((uint64_t*)pte) = 0;
			};
		};
	};
};

int vmNew()
{
	Thread *ct = getCurrentThread();
	ProcMem *oldPM = ct->pm;
	ct->pm = NULL;
	
	PML4 *pml4 = getPML4();
	pml4->entries[0].present = 0;
	pml4->entries[0].pdptPhysAddr = 0;
	
	if (oldPM != NULL)
	{
		vmDown(oldPM);
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
	pm->phys = phmAllocZeroFrame();
	
	ct->pm = pm;
	
	pml4->entries[0].pdptPhysAddr = pm->phys;
	pml4->entries[0].user = 1;
	pml4->entries[0].rw = 1;
	pml4->entries[0].present = 1;
	
	refreshAddrSpace();
	return 0;
};

static FileTree* getTree(File *fp)
{
	FileTree *ft = fp->iref.inode->ft;
	ftUp(ft);
	return ft;
};

uint64_t vmMap(uint64_t addr, size_t len, int prot, int flags, File *fp, off_t off)
{
	//vmDump(getCurrentThread()->pm, addr);
	
	if (prot & PROT_WRITE)
	{
		prot |= PROT_READ;
	};
	
	if (prot & PROT_EXEC)
	{
		prot |= PROT_READ;
	};
	
	if (flags & MAP_UN)
	{
		if (fp != NULL)
		{
			// unmappings must obviously be anonymous
			return EINVAL;
		};
	};
	
	int access;
	if (fp == NULL)
	{
		if ((flags & MAP_ANON) == 0)
		{
			// not anonymous, yet no file was specified
			return EBADF;
		};
		
		// the creator of an anonymous mapipng may both read and write it
		access = O_RDWR;
	}
	else if (fp->iref.inode->ft == NULL)
	{
		// this file cannot be mapped
		return ENODEV;
	}
	else
	{
		access = fp->oflags & O_RDWR;
	};
	
	if (flags & MAP_ANON)
	{
		if (fp != NULL)
		{
			return EINVAL;
		};
	};

	int mappingType = flags & (MAP_PRIVATE | MAP_SHARED);
	if ((mappingType != MAP_PRIVATE) && (mappingType != MAP_SHARED))
	{
		// neither private nor shared, or both
		return EINVAL;
	};

	if (mappingType == MAP_SHARED)
	{
		if (prot & PROT_WRITE)
		{
			if ((access & O_WRONLY) == 0)
			{
				// attempting to write to a file marked read-only
				return EACCES;
			};
		};
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
	
	int anonShared = (flags & MAP_SHARED) && (flags & MAP_ANON);
	
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
			seg->ft = NULL;
			if (fp != NULL) seg->ft = getTree(fp);
			if (anonShared) seg->ft = ftCreate(FT_ANON);
			seg->offset = off;
			seg->creator = creator;
			seg->flags = flags;
			seg->prot = prot;
			seg->access = access;
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
			newSeg->ft = NULL;
			if (fp != NULL) newSeg->ft = getTree(fp);
			if (anonShared) seg->ft = ftCreate(FT_ANON);
			newSeg->offset = off;
			newSeg->creator = creator;
			newSeg->flags = flags;
			newSeg->prot = prot;
			newSeg->access = access;
			
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
		
		if (flags & MAP_UN)
		{
			flags = 0;
		};
		
		// first find the segment which contains 'addr'.
		ProcMem *pm = getCurrentThread()->pm;
		semWait(&pm->lock);
		
		uint64_t pos = 0;
		Segment *seg = pm->segs;
		
		while ((pos+(seg->numPages<<12)) <= addr)
		{
			if (seg->next == NULL)
			{
				semSignal(&pm->lock);
				return ENOMEM;
			};
			
			pos += (seg->numPages << 12);
			seg = seg->next;
		};
		
		if ((pos == addr) && (seg->numPages == numPages))
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
			
			seg->ft = NULL;
			if (fp != NULL) seg->ft = getTree(fp);
			if (anonShared) seg->ft = ftCreate(FT_ANON);
			seg->offset = off;
			seg->creator = creator;
			seg->flags = flags;
			seg->prot = prot;
			seg->access = access;
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
				seg->numPages = offsetPages;
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
			seg->ft = NULL;
			if (fp != NULL) seg->ft = getTree(fp);
			if (anonShared) seg->ft = ftCreate(FT_ANON);
			seg->offset = off;
			seg->creator = creator;
			seg->flags = flags;
			seg->prot = prot;
			seg->access = access;
		};
		
		semSignal(&pm->lock);
		return addr;
	};
};

static void invalidateBlocks(uint64_t frame)
{
	Thread *ct = getCurrentThread();
	Thread *thread = ct;
	
	uint64_t rflags = getFlagsRegister();
	cli();
	lockSched();
	
	int shouldResched = 0;
	do
	{
		if ((thread->blockPhys >> 12) == frame)
		{
			thread->blockPhys = 0;
			shouldResched |= signalThread(thread);
		};
		
		thread = thread->next;
	} while (thread != ct);
	
	unlockSched();
	if (shouldResched) kyield();
	setFlagsRegister(rflags);
};

void vmFault(Regs *regs, uint64_t faultAddr, int flags)
{
	if ((faultAddr < ADDR_MIN) || (faultAddr >= ADDR_MAX))
	{
		throw(EX_PAGE_FAULT);
		
		siginfo_t si;
		memset(&si, 0, sizeof(siginfo_t));
		si.si_signo = SIGSEGV;
		si.si_code = SEGV_MAPERR;
		si.si_addr = (void*) faultAddr;
		
		cli();
		lockSched();
		sendSignal(getCurrentThread(), &si);
		switchTaskUnlocked(regs);
	};
	
	ProcMem *pm = getCurrentThread()->pm;
	semWait(&pm->lock);
	
	// try finding the segment in question
	uint64_t pos = 0;
	Segment *seg = pm->segs;
	
	while ((pos+(seg->numPages<<12)) <= faultAddr)
	{
		pos += seg->numPages << 12;
		seg = seg->next;
	};
	
	// if unmapped, send the SIGSEGV signal
	if (seg->flags == 0)
	{
		semSignal(&pm->lock);
		throw(EX_PAGE_FAULT);
		
		siginfo_t si;
		memset(&si, 0, sizeof(siginfo_t));
		si.si_signo = SIGSEGV;
		si.si_code = SEGV_MAPERR;
		si.si_addr = (void*) faultAddr;
		
		cli();
		lockSched();
		sendSignal(getCurrentThread(), &si);
		switchTaskUnlocked(regs);
	};
	
	// set permission if currently unset
	PTe *pte = getPage(faultAddr, 1);
	if (!pte->gx_perm_ovr)
	{
		pte->gx_perm_ovr = 1;
		if (seg->prot & PROT_READ)
		{
			pte->gx_r = 1;
		};
		
		if (seg->prot & PROT_WRITE)
		{
			pte->gx_w = 1;
		};
		
		if (seg->prot & PROT_EXEC)
		{
			pte->gx_x = 1;
		};
	};
	
	// check permissions
	int allowed = 1;
	
	if (flags & PF_WRITE)
	{
		if (!pte->gx_w)
		{
			allowed = 0;
		};
	};
	
	if (!pte->gx_r)
	{
		allowed = 0;
	};
	
	if (flags & PF_FETCH)
	{
		if (!pte->gx_x)
		{
			allowed = 0;
		};
	};
	
	if (!allowed)
	{
		semSignal(&pm->lock);
		throw(EX_PAGE_FAULT);
		
		if ((regs->cs & 3) == 0)
		{
			panic("page fault in kernel, address 0x%08lX", faultAddr);
		};
		
		siginfo_t si;
		memset(&si, 0, sizeof(siginfo_t));
		si.si_signo = SIGSEGV;
		si.si_code = SEGV_ACCERR;
		si.si_addr = (void*) faultAddr;
		
		cli();
		lockSched();
		sendSignal(getCurrentThread(), &si);
		switchTaskUnlocked(regs);
	};
	
	// if the page is not yet loaded, load it
	if (!pte->gx_loaded)
	{
		if (seg->ft != NULL)
		{
			off_t offset = seg->offset + ((faultAddr & ~0xFFF) - pos);
			uint64_t frame = ftGetPage(seg->ft, offset);
			
			if (frame == 0)
			{
				semSignal(&pm->lock);
				throw(EX_PAGE_FAULT);

				siginfo_t si;
				memset(&si, 0, sizeof(siginfo_t));
				si.si_signo = SIGBUS;
				si.si_code = BUS_OBJERR;
		
				cli();
				lockSched();
				sendSignal(getCurrentThread(), &si);
				switchTaskUnlocked(regs);
			};
			
			pte->framePhysAddr = frame;
		}
		else
		{
			uint64_t frame = piNew(0);
			if (frame == 0)
			{
				semSignal(&pm->lock);
				throw(EX_PAGE_FAULT);

				siginfo_t si;
				memset(&si, 0, sizeof(siginfo_t));
				si.si_signo = SIGBUS;
				si.si_code = BUS_OBJERR;
		
				cli();
				lockSched();
				sendSignal(getCurrentThread(), &si);
				switchTaskUnlocked(regs);
			};
			
			pte->framePhysAddr = frame;
		};

		pte->gx_shared = !!(seg->flags & MAP_SHARED);
		pte->gx_cow = 0;
		if (seg->flags & MAP_PRIVATE)
		{
			pte->gx_cow = 1;
			pte->rw = 0;
		}
		else if (pte->gx_w)
		{
			pte->rw = 1;
		};
		
		if (!pte->gx_x) pte->xd = 1;
		if (pte->gx_r) pte->present = 1;
		pte->user = 1;
		pte->gx_loaded = 1;
		
		// invalidate the page on the CURRENT CPU, in case we need copy-on-write
		// below
		invlpg((void*)faultAddr);
	};
	
	// check for copy-on-write faults
	if (flags & PF_WRITE)
	{
		if (pte->gx_cow)
		{
			if (piNeedsCopyOnWrite(pte->framePhysAddr))
			{
				uint64_t frame = piNew(0);
				if (frame == 0)
				{
					semSignal(&pm->lock);
					throw(EX_PAGE_FAULT);

					siginfo_t si;
					memset(&si, 0, sizeof(siginfo_t));
					si.si_signo = SIGBUS;
					si.si_code = BUS_OBJERR;
		
					cli();
					lockSched();
					sendSignal(getCurrentThread(), &si);
					switchTaskUnlocked(regs);
				};
			
				frameWrite(frame, (void*)(faultAddr & ~0xFFF));
			
				uint64_t old = pte->framePhysAddr;
				pte->framePhysAddr = frame;
				piDecref(old);
				
				invalidateBlocks(old);
			};
			
			pte->gx_cow = 0;
			pte->rw = 1;
		};
	};
	
	// finally we must invalidate the page
	invalidatePage(faultAddr);
	semSignal(&pm->lock);
};

int vmProtect(uint64_t base, size_t len, int prot)
{
	if (prot & ~PROT_ALL)
	{
		return EINVAL;
	};
	
	if (base & 0xFFF)
	{
		return EINVAL;
	};
	
	if ((base < ADDR_MIN) || (base >= ADDR_MAX))
	{
		return EINVAL;
	};
	
	if (prot & PROT_WRITE)
	{
		prot |= PROT_READ;
	};
	
	if (prot & PROT_EXEC)
	{
		prot |= PROT_READ;
	};
	
	ProcMem *pm = getCurrentThread()->pm;
	semWait(&pm->lock);
	
	uint64_t pos = 0;
	Segment *seg = pm->segs;
	
	uint64_t addr;
	for (addr=base; addr<(base+len); addr+=0x1000)
	{
		while ((pos+(seg->numPages<<12)) <= addr)
		{
			pos += seg->numPages << 12;
			seg = seg->next;
		};
		
		if (seg->flags == 0)
		{
			semSignal(&pm->lock);
			return ENOMEM;
		};
		
		// check permissions (PROT_WRITE for MAP_SHARED mappings can only be set if the file
		// was opened as writeable)
		if (prot & PROT_WRITE)
		{
			if (seg->flags & MAP_SHARED)
			{
				if ((seg->access & O_WRONLY) == 0)
				{
					// not allowed, sorry
					semSignal(&pm->lock);
					return EACCES;
				};
			};
		};
		
		// set the permissions
		PTe *pte = getPage(addr, 1);
		pte->gx_perm_ovr = 1;
		pte->gx_r = !!(prot & PROT_READ);
		pte->gx_w = !!(prot & PROT_WRITE);
		pte->gx_x = !!(prot & PROT_EXEC);
		
		if (pte->gx_loaded)
		{
			if (!pte->gx_r)
			{
				pte->present = 0;
			};
			
			if (!pte->gx_w)
			{
				pte->rw = 0;
			};
			
			if (!pte->gx_x)
			{
				pte->xd = 1;
			};
		};
		
		invalidatePage(addr);
	};
	
	return 0;
};

void vmUnmapThread()
{
	ProcMem *pm = getCurrentThread()->pm;
	
	semWait(&pm->lock);
	
	uint64_t pos = 0;
	Segment *seg = pm->segs;
	
	Thread *ct = getCurrentThread();
	while (seg != NULL)
	{
		if (seg->flags & MAP_THREAD)
		{
			if (seg->creator == ct)
			{
				unmapArea(pos, seg->numPages << 12);
				if (seg->ft != NULL)
				{
					ftDown(seg->ft);
				};
			
				seg->ft = NULL;
				seg->flags = 0;
				seg->creator = NULL;
				
				if (seg->prev != NULL)
				{
					if (seg->prev->flags == 0)
					{
						seg->prev->next = seg->next;
						if (seg->next != NULL) seg->next->prev = seg->prev;
						
						seg->prev->numPages += seg->numPages;
						
						Segment *prev = seg->prev;
						kfree(seg);
						seg = prev;
					};
				};
				
				if (seg->next != NULL)
				{
					if (seg->next->flags == 0)
					{
						seg->numPages += seg->next->numPages;
						
						Segment *next = seg->next->next;
						kfree(seg->next);
						seg->next = next;
						if (next != NULL) next->prev = seg;
					};
				};
			};
		};
		
		pos += seg->numPages << 12;
		seg = seg->next;
	};
	
	semSignal(&pm->lock);
};

static uint64_t clonePT(PT *pt)
{
	uint64_t frame = phmAllocFrame();
	
	int i;
	for (i=0; i<512; i++)
	{
		PTe *pte = &pt->entries[i];
		
		if (pte->gx_loaded)
		{
			if (!pte->gx_shared)
			{
				pte->rw = 0;
				pte->gx_cow = 1;
			};
			
			piIncref(pte->framePhysAddr);
		};
	};
	
	frameWrite(frame, pt);
	return frame;
};

static uint64_t clonePD(PD *pd)
{
	uint64_t frame = phmAllocFrame();
	PD copy;
	
	memset(&copy, 0, sizeof(PD));
	
	int i;
	for (i=0; i<512; i++)
	{
		if (pd->entries[i].present)
		{
			copy.entries[i].rw = 1;
			copy.entries[i].user = 1;
			copy.entries[i].present = 1;
			
			uint64_t ptAddr = ((uint64_t)&pd->entries[i]) << 9;
			copy.entries[i].ptPhysAddr = clonePT((PT*) ptAddr);
		};
	};
	
	frameWrite(frame, &copy);
	return frame;
};

static uint64_t clonePDPT(PDPT *pdpt)
{
	uint64_t frame = phmAllocFrame();
	PDPT copy;
	
	memset(&copy, 0, sizeof(PDPT));
	
	int i;
	for (i=0; i<512; i++)
	{
		if (pdpt->entries[i].present)
		{
			copy.entries[i].rw = 1;
			copy.entries[i].user = 1;
			copy.entries[i].present = 1;
			
			uint64_t pdAddr = ((uint64_t)&pdpt->entries[i]) << 9;
			copy.entries[i].pdPhysAddr = clonePD((PD*) pdAddr);
		};
	};
	
	frameWrite(frame, &copy);
	return frame;
};

ProcMem* vmClone()
{
	ProcMem *pm = getCurrentThread()->pm;
	ProcMem *newPM = NEW(ProcMem);
	
	semInit(&newPM->lock);
	if (pm != NULL) semWait(&pm->lock);
	
	Segment *lastSeg = NULL;
	
	// copy the segment list
	if (pm != NULL)
	{
		Segment *seg;
		for (seg=pm->segs; seg!=NULL; seg=seg->next)
		{
			Segment *newSeg = NEW(Segment);
			memcpy(newSeg, seg, sizeof(Segment));
		
			if (newSeg->ft != NULL) ftUp(newSeg->ft);
		
			// MAP_THREAD mappings become normal mappings in the new
			// address space.
			if (newSeg->flags & MAP_THREAD)
			{
				newSeg->creator = NULL;
				newSeg->flags &= ~MAP_THREAD;
			};
		
			if (lastSeg == NULL)
			{
				newSeg->prev = newSeg->next = NULL;
				newPM->segs = newSeg;
			}
			else
			{
				newSeg->prev = lastSeg;
				newSeg->next = NULL;
				lastSeg->next = newSeg;
			};
		
			lastSeg = newSeg;
		};
	}
	else
	{
		Segment *seg = NEW(Segment);
		seg->prev = seg->next = NULL;
		seg->numPages = 0x8000000;
		seg->ft = NULL;
		seg->flags = 0;
		seg->prot = 0;
	
		newPM->segs = seg;
	};
	
	newPM->refcount = 1;
	if (pm != NULL) newPM->phys = clonePDPT((PDPT*) 0xFFFFFFFFFFE00000);	/* first PDPT */
	else newPM->phys = phmAllocZeroFrame();
	refreshAddrSpace();
	
	if (pm != NULL) semSignal(&pm->lock);
	return newPM;
};

void vmUp(ProcMem *pm)
{
	__sync_fetch_and_add(&pm->refcount, 1);
};

void deletePT(uint64_t frame)
{
	PT pt;
	frameRead(frame, &pt);
	phmFreeFrame(frame);
	
	int i;
	for (i=0; i<512; i++)
	{
		if (pt.entries[i].gx_loaded)
		{
			if (pt.entries[i].dirty) piMarkDirty(pt.entries[i].framePhysAddr);
			if (pt.entries[i].accessed) piMarkAccessed(pt.entries[i].framePhysAddr);
			piDecref(pt.entries[i].framePhysAddr);
		};
	};
};

void deletePD(uint64_t frame)
{
	PD pd;
	frameRead(frame, &pd);
	phmFreeFrame(frame);
	
	int i;
	for (i=0; i<512; i++)
	{
		if (pd.entries[i].present)
		{
			deletePT(pd.entries[i].ptPhysAddr);
		};
	};
};

void deletePDPT(uint64_t frame)
{
	PDPT pdpt;
	frameRead(frame, &pdpt);
	phmFreeFrame(frame);
	
	int i;
	for (i=0; i<512; i++)
	{
		if (pdpt.entries[i].present)
		{
			deletePD(pdpt.entries[i].pdPhysAddr);
		};
	};
};

void vmDown(ProcMem *pm)
{
	if (__sync_add_and_fetch(&pm->refcount, -1) == 0)
	{
		deletePDPT(pm->phys);
		
		Segment *seg = pm->segs;
		while (seg != NULL)
		{
			Segment *next = seg->next;
			if (seg->ft != NULL) ftDown(seg->ft);
			kfree(seg);
			seg = next;
		};
	};
};

void vmSwitch(ProcMem *pm)
{
	PML4 *pml4 = getPML4();
	pml4->entries[0].pdptPhysAddr = pm->phys;
	pml4->entries[0].user = 1;
	pml4->entries[0].rw = 1;
	pml4->entries[0].present = 1;
	
	refreshAddrSpace();
};

void vmDump(ProcMem *pm, uint64_t addr)
{
	uint64_t pos = 0;
	Segment *seg;
	for (seg=pm->segs; seg!=NULL; seg=seg->next)
	{
		char prefix = ' ';
		if ((pos <= addr) && ((pos + (seg->numPages << 12)) > addr))
		{
			prefix = '>';
		};
		
		if (seg->flags == 0)
		{
			kprintf("%c0x%016lx - 0x%016lx UNMAPPED\n", prefix, pos, pos + (seg->numPages << 12) - 1);
		}
		else
		{
			kprintf("%c0x%016lx - 0x%016lx %c%c %c%c%c %c @0x%016lX\n",
				prefix,
				pos, pos + (seg->numPages << 12) - 1,
				(seg->flags & MAP_SHARED) ? 'S' : 'P',
				(seg->flags & MAP_THREAD) ? 'T' : 't',
				(seg->prot & PROT_READ) ? 'R' : 'r',
				(seg->prot & PROT_WRITE) ? 'W' : 'w',
				(seg->prot & PROT_EXEC) ? 'X' : 'x',
				(seg->access & O_WRONLY) ? 'W' : 'R',
				seg->offset);
		};
		
		pos += seg->numPages << 12;
	};
};

uint64_t vmGetPhys(uint64_t faultAddr, int requiredPerms)
{
	if ((faultAddr < ADDR_MIN) || (faultAddr >= ADDR_MAX))
	{
		return 0;
	};
	
	ProcMem *pm = getCurrentThread()->pm;
	semWait(&pm->lock);
	
	// try finding the segment in question
	uint64_t pos = 0;
	Segment *seg = pm->segs;
	
	while ((pos+(seg->numPages<<12)) <= faultAddr)
	{
		pos += seg->numPages << 12;
		seg = seg->next;
	};
	
	if (seg->flags == 0)
	{
		semSignal(&pm->lock);
		return 0;
	};
	
	// set permission if currently unset
	PTe *pte = getPage(faultAddr, 1);
	if (!pte->gx_perm_ovr)
	{
		pte->gx_perm_ovr = 1;
		if (seg->prot & PROT_READ)
		{
			pte->gx_r = 1;
		};
		
		if (seg->prot & PROT_WRITE)
		{
			pte->gx_w = 1;
		};
		
		if (seg->prot & PROT_EXEC)
		{
			pte->gx_x = 1;
		};
	};
	
	// check permissions
	int allowed = 1;
	
	if (requiredPerms & PROT_WRITE)
	{
		if (!pte->gx_w)
		{
			allowed = 0;
		};
	};
	
	if (!pte->gx_r)
	{
		allowed = 0;
	};
	
	if (requiredPerms & PROT_EXEC)
	{
		if (!pte->gx_x)
		{
			allowed = 0;
		};
	};
	
	if (!allowed)
	{
		semSignal(&pm->lock);
		return 0;
	};
	
	// if the page is not yet loaded, load it
	if (!pte->gx_loaded)
	{
		if (seg->ft != NULL)
		{
			off_t offset = seg->offset + ((faultAddr & ~0xFFF) - pos);
			uint64_t frame = ftGetPage(seg->ft, offset);
			
			if (frame == 0)
			{
				semSignal(&pm->lock);
				return 0;
			};
			
			pte->framePhysAddr = frame;
		}
		else
		{
			uint64_t frame = piNew(0);
			if (frame == 0)
			{
				semSignal(&pm->lock);
				return 0;
			};
			
			pte->framePhysAddr = frame;
		};

		pte->gx_shared = !!(seg->flags & MAP_SHARED);
		pte->gx_cow = 0;
		if (seg->flags & MAP_PRIVATE)
		{
			pte->gx_cow = 1;
			pte->rw = 0;
		}
		else if (pte->gx_w)
		{
			pte->rw = 1;
		};
		
		if (!pte->gx_x) pte->xd = 1;
		if (pte->gx_r) pte->present = 1;
		pte->user = 1;
		pte->gx_loaded = 1;
		
		// invalidate the page on the CURRENT CPU, in case we need copy-on-write
		// below
		invlpg((void*)faultAddr);
	};
	
	// check for copy-on-write faults
	if (pte->gx_cow)
	{
		if (piNeedsCopyOnWrite(pte->framePhysAddr))
		{
			uint64_t frame = piNew(0);
			if (frame == 0)
			{
				semSignal(&pm->lock);
				return 0;
			};
		
			frameWrite(frame, (void*)(faultAddr & ~0xFFF));
		
			uint64_t old = pte->framePhysAddr;
			pte->framePhysAddr = frame;
			piDecref(old);
		};
		
		pte->gx_cow = 0;
		pte->rw = 1;
	};
	
	// finally we must invalidate the page
	invalidatePage(faultAddr);
	uint64_t result = pte->framePhysAddr;
	piIncref(result);
	semSignal(&pm->lock);
	
	return result;
};
