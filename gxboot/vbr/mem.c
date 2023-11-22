/*
	Glidix bootloader (gxboot)

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

#include "mem.h"

static uint32_t numMemoryMapEnts;

static MemorySlice* memoryTable;
static uint32_t numMemorySlices;

uint64_t *pml4;

static void initMemoryTable()
{
	dtermput("Initializing the memory table... ");

	MemorySlice *nextEntry = (MemorySlice*) MEMTAB_ADDR;
	memoryTable = nextEntry;
	numMemorySlices = 0;

	for (uint32_t i=0; i<numMemoryMapEnts; i++)
	{
		MemoryMap *mmap = (MemoryMap*) BIOSMAP_ADDR + i;
		if (mmap->type != 1)
		{
			// Not normal RAM.
			continue;
		};

		if (mmap->baseAddr + mmap->len <= INITIAL_PLACEMENT)
		{
			// The memory is below the initial placement.
			continue;
		};

		nextEntry->baseAddr = mmap->baseAddr;
		nextEntry->len = mmap->len;
		
		if (nextEntry->baseAddr < INITIAL_PLACEMENT)
		{
			// Needs fixing up.
			uint64_t delta = INITIAL_PLACEMENT - nextEntry->baseAddr;
			nextEntry->len -= delta;
			nextEntry->baseAddr = INITIAL_PLACEMENT;
		};

		nextEntry++;
		numMemorySlices++;
	};

	dtermput("OK\n");
};

void memInit()
{
	// Read the memory map.
	MemoryMap *mmapPut = (MemoryMap*) BIOSMAP_ADDR;
	uint32_t entIndex = 0;

	numMemoryMapEnts = 0;

	dtermput("Loading memory map... ");
	do
	{
		int ok = 1;
		entIndex = biosGetMap(entIndex, &mmapPut->baseAddr, &ok);
		if (!ok) break;
		
		mmapPut->size = 20;
		mmapPut++;
		numMemoryMapEnts++;
	} while (entIndex != 0);

	dtermput("OK\n");

	initMemoryTable();

	// Allocate space for the PML4.
	pml4 = (uint64_t*) balloc(0x1000, 0x1000);
	memset(pml4, 0, 0x1000);
	
	// Recursive mapping.
	pml4[511] = (uint64_t) (uint32_t) pml4 | PT_WRITE | PT_PRESENT;

	// Identity-map the RAM areas.
	MemoryMap *map = (MemoryMap*) BIOSMAP_ADDR;
	for (uint32_t i=0; i<numMemoryMapEnts; i++)
	{
		MemoryMap *ent = &map[i];
		if (ent->type != 1)
		{
			// Not normal RAM.
			continue;
		};

		mmap(PHYS_MAP_BASE + ent->baseAddr, ent->baseAddr, ent->len);
	};
};

void* balloc(uint32_t align, uint32_t size)
{
	uint64_t align64 = (uint64_t) align;

	MemorySlice *bestSlice = NULL;
	int minWaste = 0;

	for (MemorySlice *slice=memoryTable; slice!=memoryTable+numMemorySlices; slice++)
	{
		uint64_t aligned = (slice->baseAddr + align64 - 1) & ~(align64 - 1);
		uint64_t delta = aligned - slice->baseAddr;

		if (aligned + size > (1ULL << 32))
		{
			// Not suitable for 32-bit allocations.
			continue;
		};

		if (aligned + size > slice->len)
		{
			// Not large enough.
			continue;
		};

		if (bestSlice == NULL || minWaste > delta)
		{
			bestSlice = slice;
			minWaste = delta;
		};
	};

	if (bestSlice == NULL)
	{
		termput("ERROR: Out of memory.");

		for (;;)
		{
			asm volatile ("hlt");
		};
	};

	bestSlice->baseAddr += minWaste;
	bestSlice->len -= minWaste;

	uint32_t addr = (uint32_t) bestSlice->baseAddr;
	bestSlice->baseAddr += size;
	bestSlice->len -= size;

	return (void*) addr;
};

uint64_t memGetBiosMapSize()
{
	return 24 * numMemoryMapEnts;
};

static uint64_t *getPageEntry(uint64_t addr)
{
	uint64_t pageIndex = (addr >> 12) & 0x1FFULL;
	uint64_t ptIndex = (addr >> 21) & 0x1FFULL;
	uint64_t pdIndex = (addr >> 30) & 0x1FFULL;
	uint64_t pdptIndex = (addr >> 39) & 0x1FFULL;
	
	if (pdptIndex == 511)
	{
		termput("ERROR: Attempting to access the top of 64-bit virtual memory, reserved for recursive mapping!");
		while (1) asm volatile ("cli; hlt");
	};
	
	uint64_t *pdpt;
	if (pml4[pdptIndex] == 0)
	{
		pdpt = (uint64_t*) balloc(0x1000, 0x1000);
		memset(pdpt, 0, 0x1000);
		pml4[pdptIndex] = (uint64_t)(uint32_t)pdpt | PT_PRESENT | PT_WRITE;
	}
	else
	{
		pdpt = (uint64_t*) ((uint32_t) (pml4[pdptIndex] & ~0xFFFULL));
	};
	
	uint64_t *pd;
	if (pdpt[pdIndex] == 0)
	{
		pd = (uint64_t*) balloc(0x1000, 0x1000);
		memset(pd, 0, 0x1000);
		pdpt[pdIndex] = (uint64_t)(uint32_t)pd | PT_PRESENT | PT_WRITE;
	}
	else
	{
		pd = (uint64_t*) ((uint32_t) (pdpt[pdIndex] & ~0xFFFULL));
	};
	
	uint64_t *pt;
	if (pd[ptIndex] == 0)
	{
		pt = (uint64_t*) balloc(0x1000, 0x1000);
		memset(pt, 0, 0x1000);
		pd[ptIndex] = (uint64_t)(uint32_t)pt | PT_PRESENT | PT_WRITE;
	}
	else
	{
		pt = (uint64_t*) ((uint32_t) (pd[ptIndex] & ~0xFFF));
	};
	
	return &pt[pageIndex];
};

void mmap(uint64_t vaddr, uint64_t paddr, uint64_t size)
{
	size = (size + 0xFFF) & ~0xFFF;
	
	termput("mmap ");
	termputp64(vaddr);
	termput(" -> ");
	termputp64(paddr);
	termput(" (");
	termputp64((uint64_t) size);
	termput(")\n");
	
	uint64_t i;
	for (i=0; i<size; i+=0x1000)
	{	
		uint64_t *pte = getPageEntry(vaddr+i);
		*pte = (paddr+i) | PT_PRESENT | PT_WRITE;
	};
};

void* virt2phys(uint64_t virt)
{
	uint64_t *pte = getPageEntry(virt);
	if (*pte == 0)
	{
		return NULL;
	};
	
	uint32_t addr = ((uint32_t)(*pte) & (~0xFFF)) | (virt & 0xFFF);
	return (void*) addr;
};