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

#include <glidix/hw/buddy.h>
#include <glidix/display/console.h>
#include <glidix/util/common.h>
#include <glidix/util/string.h>

static int numMemoryRegions;
static MemoryRegion regions[BUDDY_MAX_REGIONS];
static uint64_t totalUseableMemory;

static void buddyAddRegion(uint64_t base, uint64_t len)
{
	if (numMemoryRegions == BUDDY_MAX_REGIONS)
	{
		FAILED();
		panic("too many memory regions (this is likely a bug)");
	};

	MemoryRegion *region = &regions[numMemoryRegions++];

	// Initialize the bitmap.
	region->numPages = (len + 0xFFF) >> 12;
	region->bitmap = (uint8_t*) (base + PHYS_MAP_BASE);

	size_t bitmapSize = ((2 * region->numPages) + 7) >> 3;
	memset(region->bitmap, 0, bitmapSize);

	base += bitmapSize;
	len -= bitmapSize;

	// Page-align the slice.
	if (base & 0xFFF)
	{
		uint64_t delta = 0x1000 - (base & 0xFFF);
		base += delta;
		len -= delta;
	};

	region->base = base + PHYS_MAP_BASE;

	// TODO: initialize the bitmap by marking unuseable positions as permanently unavailable.

	totalUseableMemory += len;
};

void buddyInit()
{
	kprintf("Initializing the buddy allocator...");
	for (uint64_t i=0; i<bootInfo->memtabCount; i++)
	{
		BootMemorySlice *slice = &bootInfo->memtab[i];
		buddyAddRegion(slice->base, slice->len);
	};

	DONE();

	kprintf("  Total number of regions: %d\n", numMemoryRegions);
	kprintf("  Total useable memory: %luM\n", totalUseableMemory / 1024 / 1024);

	while (1);
};