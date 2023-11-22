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

	dtermput("Initial memory table: \n");
	for (uint32_t i=0; i<numMemorySlices; i++)
	{
		MemorySlice *slice = &memoryTable[i];
		dtermput("  ");
		dtermputp64(slice->baseAddr);
		dtermput(" - ");
		dtermputp64(slice->baseAddr + slice->len);
		dtermput("\n");
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