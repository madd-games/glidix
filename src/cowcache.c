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

#include <glidix/cowcache.h>
#include <glidix/spinlock.h>
#include <glidix/physmem.h>
#include <glidix/isp.h>
#include <glidix/console.h>
#include <glidix/string.h>

static PAGE_ALIGN PDPT cowPDPT;
static PAGE_ALIGN PD cowFirstPD;
static PAGE_ALIGN PT cowPTPageDirs;
static Spinlock cowLock;

void cowInit()
{
	cowPDPT.entries[0].present = 1;
	cowPDPT.entries[0].pdPhysAddr = (((uint64_t)&cowFirstPD-0xFFFF800000000000) >> 12);

	cowFirstPD.entries[0].present = 1;
	cowFirstPD.entries[0].ptPhysAddr = (((uint64_t)&cowPTPageDirs-0xFFFF800000000000) >> 12);

	PML4 *pml4 = getPML4();
	pml4->entries[260].present = 1;
	pml4->entries[260].pdptPhysAddr = (((uint64_t)&cowPDPT-0xFFFF800000000000) >> 12);

	refreshAddrSpace();
	spinlockRelease(&cowLock);
};

int cowCreateFrameInTable(COWFrame *cf, uint64_t frame, int diridx, int tabidx)
{
	kprintf_debug("create frame in directory %d table %d\n", diridx, tabidx);
	PT *tables = (PT*) (0xFFFF820000000000 + (diridx * 0x40000000));
	PT *pt = &tables[tabidx];

	// refcounts for this table
	uint64_t *refcounts = (uint64_t*) (0xFFFF820000000000 + (diridx * 0x200000) + (tabidx * 0x1000));

	int i;
	for (i=0; i<512; i++)
	{
		if (refcounts[i] == 0)
		{
			refcounts[i] = 1;
			pt->entries[i].present = 1;
			pt->entries[i].framePhysAddr = frame;
			refreshAddrSpace();

			cf->refcountptr = &refcounts[i];
			cf->pte = &pt->entries[i];
			cf->data = (void*) (0xFFFF820000000000 + (diridx * 0x40000000) + (tabidx * 0x200000) + (i * 0x1000));
			return 0;
		};
	};

	return -1;
};

int cowCreateFrameInDirectory(COWFrame *cf, uint64_t frame, int diridx)
{
	PD *pd = (PD*) (0xFFFF820000000000 + (diridx * 0x1000));
	PT *ptTables = (PT*) (0xFFFF820000000000 + (diridx * 0x40000000));
	int i;

	for (i=1; i<512; i++)
	{
		if (!pd->entries[i].present)
		{
			// non-allocated page table, let's allocate it.
			uint64_t ptPhysFrame = phmAllocFrame();
			//ispLock();
			//ispSetFrame(ptPhysFrame);
			//memset(ispGetPointer(), 0, 0x1000);
			//ispUnlock();

			ptTables->entries[i].present = 1;
			ptTables->entries[i].framePhysAddr = ptPhysFrame;
			refreshAddrSpace();

			memset(&ptTables[i], 0, sizeof(PT));

			pd->entries[i].present = 1;
			pd->entries[i].ptPhysAddr = ptPhysFrame;
			refreshAddrSpace();

			return cowCreateFrameInTable(cf, frame, diridx, i);
		}
		else
		{
			if (cowCreateFrameInTable(cf, frame, diridx, i) == 0)
			{
				return 0;
			};
		};
	};

	return -1;
};

void cowCreateFrame(COWFrame *cf, uint64_t frame)
{
	spinlockAcquire(&cowLock);

	int i;
	for (i=1; i<512; i++)
	{
		if (!cowPTPageDirs.entries[i].present)
		{
			// Directory i does not exist, create it.
			uint64_t pdPhysFrame = phmAllocFrame();
			ispLock();
			ispSetFrame(pdPhysFrame);
			memset(ispGetPointer(), 0, 0x1000);
			ispUnlock();

			cowPTPageDirs.entries[i].present = 1;
			cowPTPageDirs.entries[i].framePhysAddr = pdPhysFrame;
			refreshAddrSpace();

			uint64_t pdVirtAddr = 0xFFFF820000000000 + (i * 0x1000);
			PD *pd = (PD*) pdVirtAddr;
			memset(pd, 0, sizeof(PD));

			cowPDPT.entries[i].present = 1;
			cowPDPT.entries[i].pdPhysAddr = pdPhysFrame;
			refreshAddrSpace();

			// Make the first page table, which shall map to page tables for this directory.
			uint64_t ptPhysAddr = phmAllocFrame();

			ispLock();
			ispSetFrame(ptPhysAddr);
			PT *pt = (PT*) ispGetPointer();
			memset(pt, 0, sizeof(PT));
			pt->entries[0].present = 1;
			pt->entries[0].framePhysAddr = ptPhysAddr;
			ispUnlock();

			pd->entries[0].present = 1;
			pd->entries[0].ptPhysAddr = ptPhysAddr;
			refreshAddrSpace();

			// create the page table in cowFirstPD to keep refcounts for this page dir
			ptPhysAddr = phmAllocFrame();
			ispLock();
			ispSetFrame(ptPhysAddr);
			pt = (PT*) ispGetPointer();
			memset(pt, 0, sizeof(PT));
			int j;
			for (j=0; j<512; j++)
			{
				pt->entries[j].present = 1;
				pt->entries[j].framePhysAddr = phmAllocFrame();
			};
			ispUnlock();

			cowFirstPD.entries[i].present = 1;
			cowFirstPD.entries[i].ptPhysAddr = ptPhysAddr;
			refreshAddrSpace();

			// set all refcounts to 0 initially.
			void *refcountArea = (void*) (0xFFFF820000000000 + (i * 0x200000));
			memset(refcountArea, 0, 0x200000);

			cowCreateFrameInDirectory(cf, frame, i);
			spinlockRelease(&cowLock);
			return;
		}
		else
		{
			if (cowCreateFrameInDirectory(cf, frame, i) == 0)
			{
				spinlockRelease(&cowLock);
				return;
			};
		};
	};

	panic("copy-on-write cache overflow");
};

void cowPrintFrame(COWFrame *cf)
{
	kprintf("%a\t%a\t%a\t%d\n", cf->refcountptr, cf->pte, cf->data, *(cf->refcountptr));
};
