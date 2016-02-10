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

#include <glidix/isp.h>
#include <glidix/common.h>
#include <glidix/pagetab.h>
#include <glidix/memory.h>
#include <glidix/string.h>
#include <glidix/spinlock.h>
#include <stdint.h>

static PTe *ispPTE;
static Spinlock ispSpinlock;

void ispInit()
{
	// TODO: xd (execute disable, not the stupid face), we'll look at that shit in a bit.

	// note that for now, kxmalloc() returns physical addresses with the virtual base
	// added.
	PML4 *pml4 = getPML4();
	PDPT *pdpt = kxmalloc(sizeof(PDPT), MEM_PAGEALIGN);
	memset(pdpt, 0, sizeof(PDPT));
	PD *pd = kxmalloc(sizeof(PD), MEM_PAGEALIGN);
	memset(pd, 0, sizeof(PD));
	pdpt->entries[0].present = 1;
	pdpt->entries[0].rw = 1;
	pdpt->entries[0].pdPhysAddr = (((uint64_t)pd-0xFFFF800000000000) >> 12);

	PT *pt = kxmalloc(sizeof(PT), MEM_PAGEALIGN);
	memset(pt, 0, sizeof(PT));
	pd->entries[0].present = 1;
	pd->entries[0].rw = 1;
	pd->entries[0].ptPhysAddr = (((uint64_t)pt-0xFFFF800000000000) >> 12);

	// get the page for virtual address 0x18000000000.
	ispPTE = &pt->entries[0];
	ispPTE->present = 1;
	ispPTE->rw = 1;
	ispPTE->framePhysAddr = 0;

	// APIC register space - also make sure it is in the default place.
	//msrWrite(0x1B, 0xFEE00000 | (1 << 11) /*| (1 << 8)*/ );
	pt->entries[1].present = 1;
	pt->entries[1].framePhysAddr = 0xFEE00;
	pt->entries[1].pcd = 1;

	// IOAPIC register space
	pt->entries[2].present = 1;
	pt->entries[2].framePhysAddr = 0xFEC00;
	pt->entries[2].pcd = 1;

	// set it in the PML4 (so it maps from 0xFFFF808000000000 up).
	pml4->entries[257].present = 1;
	pml4->entries[257].pdptPhysAddr = (((uint64_t)pdpt-0xFFFF800000000000) >> 12);

	// refresh the address space
	refreshAddrSpace();

	spinlockRelease(&ispSpinlock);
};

void *ispGetPointer()
{
	return (void*) 0xFFFF808000000000;
};

void ispSetFrame(uint64_t frame)
{
	ispPTE->framePhysAddr = frame;
	refreshAddrSpace();
};

void ispLock()
{
	spinlockAcquire(&ispSpinlock);
};

void ispUnlock()
{
	spinlockRelease(&ispSpinlock);
};

void pmem_read(void *buffer, uint64_t physAddr, size_t len)
{
	ispLock();
	while (len != 0)
	{
		uint64_t frame = physAddr / 0x1000;
		uint64_t offset = physAddr % 0x1000;
		size_t toCopy = 0x1000 - offset;
		if (toCopy > len) toCopy = len;

		ispSetFrame(frame);
		memcpy(buffer, (void*)((uint64_t)ispGetPointer() + offset), toCopy);
		len -= toCopy;
		buffer = (void*)((uint64_t)buffer+toCopy);
	};
	ispUnlock();
};

void pmem_write(uint64_t physAddr, const void *buffer, size_t len)
{
	ispLock();
	while (len != 0)
	{
		uint64_t frame = physAddr / 0x1000;
		uint64_t offset = physAddr % 0x1000;
		size_t toCopy = 0x1000 - offset;
		if (toCopy > len) toCopy = len;

		ispSetFrame(frame);
		memcpy((void*)((uint64_t)ispGetPointer() + offset), buffer, toCopy);
		len -= toCopy;
		buffer = (void*)((uint64_t)buffer+toCopy);
	};
	ispUnlock();
};
