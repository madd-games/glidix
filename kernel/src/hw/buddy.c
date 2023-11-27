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
#include <glidix/thread/spinlock.h>

static int numMemoryRegions;
static MemoryRegion regions[BUDDY_MAX_REGIONS];
static uint64_t totalUseableMemory;
static Spinlock buddyLock;

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

	uint64_t currentBitmapOffset = 0;
	uint64_t nextBase = region->base;

	for (int order=0; order<BUDDY_NUM_ORDERS; order++)
	{
		MemoryBucket *bucket = &region->buckets[order];
		bucket->bitmapOffset = currentBitmapOffset;

		uint64_t blockCount = (region->numPages + (1 << order) - 1) >> order;

		currentBitmapOffset += blockCount;

		if (len & (0x1000UL << order))
		{
			FreeMemoryHeader *freeHead = (FreeMemoryHeader*) nextBase;
			nextBase += 0x1000UL << order;
			freeHead->prev = freeHead->next = NULL;

			bucket->freeHead = freeHead;
		}
		else
		{
			bucket->freeHead = NULL;
		};
	};

	totalUseableMemory += len;
};

void buddyInit()
{
	kprintf("Initializing the buddy allocator... ");
	for (uint64_t i=0; i<bootInfo->memtabCount; i++)
	{
		BootMemorySlice *slice = &bootInfo->memtab[i];
		buddyAddRegion(slice->base, slice->len);
	};

	DONE();

	kprintf("  Total number of regions: %d\n", numMemoryRegions);
	kprintf("  Total useable memory: %luM\n", totalUseableMemory / 1024 / 1024);

	spinlockRelease(&buddyLock);

	// DEBUG
	for (;;)
	{
		kprintf("P = %p\n", buddyAlloc(0));
	};
};

static void buddyMarkBlockUsed(MemoryRegion *region, uint64_t offset, int order)
{
	uint64_t index = region->buckets[order].bitmapOffset + (offset >> (12 + order));
	region->bitmap[index / 8] |= (1 << (index % 8));
};

static void* buddyAllocUnlocked(int order, MemoryRegion **outRegion)
{
	if (order == BUDDY_NUM_ORDERS)
	{
		return NULL;
	};

	for (uint64_t i=0; i<numMemoryRegions; i++)
	{
		MemoryRegion *region = &regions[i];
		MemoryBucket *bucket = &region->buckets[order];

		if (bucket->freeHead != NULL)
		{
			FreeMemoryHeader *head = bucket->freeHead;
			bucket->freeHead = head->next;
			if (head->next != NULL)
			{
				head->next->prev = NULL;
			};

			uint64_t offset = (uint64_t) head - region->base;
			buddyMarkBlockUsed(region, offset, order);

			*outRegion = region;
			return head;
		};
	};

	MemoryRegion *region;
	uint8_t *higherBlock = buddyAllocUnlocked(order+1, &region);
	if (higherBlock == NULL)
	{
		return NULL;
	};

	MemoryBucket *bucket = &region->buckets[order];

	uint64_t offset = (uint64_t) higherBlock - region->base;
	buddyMarkBlockUsed(region, offset, order);

	FreeMemoryHeader *other = (FreeMemoryHeader*) (higherBlock + (0x1000 << order));
	other->prev = NULL;
	other->next = bucket->freeHead;
	if (bucket->freeHead != NULL)
	{
		bucket->freeHead->prev = other;
	};

	bucket->freeHead = other;

	*outRegion = region;
	return higherBlock;
};

void* buddyAlloc(int order)
{
	uint64_t flags = getFlagsRegister();

	cli();
	spinlockAcquire(&buddyLock);

	MemoryRegion *region;
	void *block = buddyAllocUnlocked(order, &region);

	spinlockRelease(&buddyLock);
	setFlagsRegister(flags);

	return block;
};