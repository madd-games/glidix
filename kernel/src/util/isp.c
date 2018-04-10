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

#include <glidix/util/isp.h>
#include <glidix/util/common.h>
#include <glidix/hw/pagetab.h>
#include <glidix/util/memory.h>
#include <glidix/util/string.h>
#include <glidix/thread/spinlock.h>
#include <stdint.h>

static PTe *ispPTE;
static Spinlock ispSpinlock;
static PAGE_ALIGN PT ispPT;
static PAGE_ALIGN PD ispPD;
static PAGE_ALIGN PDPT ispPDPT;

extern char usup_start;

void ispInit()
{
	// TODO: xd (execute disable, not the stupid face), we'll look at that shit in a bit.
	PML4 *pml4 = getPML4();
	PDPT *pdpt = &ispPDPT;
	memset(pdpt, 0, sizeof(PDPT));
	PD *pd = &ispPD;
	memset(pd, 0, sizeof(PD));
	pdpt->entries[0].present = 1;
	pdpt->entries[0].rw = 1;
	pdpt->entries[0].user = 1;		// because of the user support page
	pdpt->entries[0].pdPhysAddr = VIRT_TO_FRAME(pd);

	PT *pt = &ispPT;
	memset(pt, 0, sizeof(PT));
	pd->entries[0].present = 1;
	pd->entries[0].rw = 1;
	pd->entries[0].user = 1;		// because of the user support page
	pd->entries[0].ptPhysAddr = VIRT_TO_FRAME(pt);

	// get the page for ISP.
	ispPTE = &pt->entries[0];
	ispPTE->present = 1;
	ispPTE->rw = 1;
	ispPTE->framePhysAddr = 0;

	// APIC register space - also make sure it is in the default place.
	pt->entries[1].present = 1;
	pt->entries[1].framePhysAddr = 0xFEE00;
	pt->entries[1].pcd = 1;
	pt->entries[1].rw = 1;

	// 2 unused, for now.
	
	// user support page; executable userspace code
	pt->entries[3].present = 1;
	pt->entries[3].framePhysAddr = VIRT_TO_FRAME(&usup_start);
	pt->entries[3].user = 1;

	/**
	 * NOTE: 4-19 inclusive reserved for I/O APICs.
	 */
	
	// set it in the PML4 (so it maps from 0xFFFF808000000000 up).
	pml4->entries[257].present = 1;
	pml4->entries[257].pdptPhysAddr = VIRT_TO_FRAME(pdpt);
	pml4->entries[257].rw = 1;
	pml4->entries[257].user = 1;		// beucase of the user support page
	
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
