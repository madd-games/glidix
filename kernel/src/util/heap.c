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

#include <glidix-config.h>

#ifdef __CONFIG_HEAP_SLAB

#include <glidix/util/memory.h>
#include <glidix/thread/mutex.h>
#include <glidix/display/console.h>
#include <glidix/hw/pagetab.h>
#include <glidix/util/string.h>
#include <glidix/hw/physmem.h>
#include <glidix/util/isp.h>
#include <glidix/storage/storage.h>
#include <glidix/util/random.h>
#include <stdint.h>

#define	HEAP_BASE_ADDR				0xFFFF810000000000
#define	NUM_BUCKETS				26

static Mutex heapLock;

/**
 * Header used on a free slab. They form a linked list with equal-sized neighbours.
 */
typedef struct FreeSlab_
{
	struct FreeSlab_ *next;
} FreeSlab;

/**
 * Header on a used slab.
 */
typedef struct
{
	/**
	 * Index of the bucket in which this slab belongs.
	 */
	uint64_t				bucket;
	
	/**
	 * Actual requested size.
	 */
	uint64_t				realSize;
	
	/**
	 * Slab data.
	 */
	char					data[];
} UsedSlab;

/**
 * A table of free slab chains of different sizes, called "buckets". The index of a bucket, n, determines the
 * size of slabs it contains:
 *
 *	slabSize = 2^(n+5)
 *
 * So bucket 0 contains slabs with 32 bytes, bucket 25 contains 1GB slabs. There are 26 buckets because 1GB is
 * also the maximum size reserved for the heap.
 */
static FreeSlab* buckets[NUM_BUCKETS];

/**
 * Ensure that the specified page range is mapped.
 */
static void mapPages(void *ptr_, size_t size)
{
	uint64_t ptr = (uint64_t) ptr_;
	uint64_t addr;
	for (addr=ptr; addr<(ptr+size); addr+=0x1000)
	{
		PML4e *pml4e = VIRT_TO_PML4E(addr);
		if (!pml4e->present)
		{
			pml4e->pdptPhysAddr = phmAllocZeroFrame();
			pml4e->rw = 1;
			pml4e->present = 1;
			
			refreshAddrSpace();
		};
		
		PDPTe *pdpte = VIRT_TO_PDPTE(addr);
		if (!pdpte->present)
		{
			pdpte->pdPhysAddr = phmAllocZeroFrame();
			pdpte->rw = 1;
			pdpte->present = 1;
			
			refreshAddrSpace();
		};
		
		PDe *pde = VIRT_TO_PDE(addr);
		if (!pde->present)
		{
			pde->ptPhysAddr = phmAllocZeroFrame();
			pde->rw = 1;
			pde->present = 1;
			
			refreshAddrSpace();
		};
		
		PTe *pte = VIRT_TO_PTE(addr);
		if (!pte->present)
		{
			pte->framePhysAddr = phmAllocFrame();
			pte->rw = 1;
			pte->present = 1;
			
			invlpg((void*)addr);
		};
	};
};

/**
 * Allocate a free slab from the specified bucket and remove it from said bucket. If there are no slabs in this
 * bucket, take a slab from the next bucket up and split it in half. If there are no more slabs available at all,
 * return NULL.
 *
 * The returned slab is only guaranteed to have the first page of it mapped.
 */
static FreeSlab* slabAlloc(int bucket)
{
	if (bucket >= NUM_BUCKETS) return NULL;
	
	size_t slabSize = 1UL << (bucket+5);
	if (buckets[bucket] != NULL)
	{
		FreeSlab *front = buckets[bucket];
		buckets[bucket] = front->next;
		return front;
	}
	else
	{
		FreeSlab *higher = slabAlloc(bucket+1);
		if (higher == NULL) return NULL;
		
		FreeSlab *other = (FreeSlab*) ((uint64_t) higher + slabSize);
		mapPages(other, 1);
		other->next = NULL;
		buckets[bucket] = other;
		
		return higher;
	};
};

/**
 * Initialize the heap. This is done by adding a single big slab.
 */
void initMemoryPhase2()
{
	FreeSlab *initSlab = (FreeSlab*) HEAP_BASE_ADDR;
	mapPages(initSlab, 1);
	initSlab->next = NULL;
	buckets[NUM_BUCKETS-1] = initSlab;
	
	readyForDynamic = 1;
};

/**
 * Allocate memory.
 */
void *kxmallocDynamic(size_t size, int flags, const char *aid, int lineno)
{
	// TODO: don't ignore the flags!
	if (size == 0) return NULL;
	
	// make room for the used slab header
	size += 16;
	
	int bucket;
	for (bucket=0; bucket<NUM_BUCKETS; bucket++)
	{
		if ((1UL << (bucket+5)) >= size)
		{
			break;
		};
	};
	
	if (bucket == NUM_BUCKETS)
	{
		return NULL;
	};
	
	mutexLock(&heapLock);
	FreeSlab *fslab = slabAlloc(bucket);
	if (fslab == NULL)
	{
		mutexUnlock(&heapLock);
		return NULL;
	};
	mapPages(fslab, size);
	mutexUnlock(&heapLock);
	
	UsedSlab *uslab = (UsedSlab*) fslab;
	uslab->bucket = bucket;
	uslab->realSize = size - 16;
	
	return uslab->data;
};

/**
 * Free a block of memory that was previously allocated using kxmallocDynamic().
 */
void _kfree(void *block, const char *who, int line)
{
	if (block == NULL) return;
	
	UsedSlab *uslab = (UsedSlab*) block;
	uslab--;
	
	FreeSlab *fslab = (FreeSlab*) uslab;
	int bucket = (int) uslab->bucket;
	
	mutexLock(&heapLock);
	fslab->next = buckets[bucket];
	buckets[bucket] = fslab;
	mutexUnlock(&heapLock);
};

/**
 * Re-allocate a block of memory (basically the kernel equivalent of realloc()).
 */
void *krealloc(void *block, size_t size)
{
	if (size == 0)
	{
		kfree(block);
		return NULL;
	};
	
	if (block == NULL)
	{
		return kmalloc(size);
	};
	
	// make space for the header
	size += 16;
	
	// get the header
	UsedSlab *slab = (UsedSlab*) block;
	slab--;
	
	size_t currentSize = (1UL << (slab->bucket+5));
	
	// if shrinking below the size of the previous bucket, split this slab in half
	if (slab->bucket != 0 && size < (currentSize>>1))
	{
		slab->bucket--;
		slab->realSize = size-16;
		
		size_t newBucketSize = (1UL << (slab->bucket+5));
		FreeSlab *second = (FreeSlab*) ((uint64_t) slab + newBucketSize);
		mapPages(second, 1);
		
		mutexLock(&heapLock);
		second->next = buckets[slab->bucket];
		buckets[slab->bucket] = second;
		mutexUnlock(&heapLock);
		
		return block;
	};
	
	// in all other cases, first check if we already have enough memory
	if (size <= currentSize)
	{
		slab->realSize = size-16;
		mapPages(slab, size);
		return block;
	};
	
	// worst case: slab not big enough, do a full reallocation
	void *newblock = kmalloc(size-16);
	if (newblock == NULL) return NULL;
	
	size_t toCopy = slab->realSize;
	if (toCopy > (size-16)) toCopy = size-16;
	memcpy(newblock, block, toCopy);
	
	kfree(block);
	return newblock;
};

/**
 * Dump the heap status.
 */
void heapDump()
{
	static const char *bucketLabels[NUM_BUCKETS] = {
		"32 bytes",			/* 0 */
		"64 bytes",			/* 1 */
		"128 bytes",			/* 2 */
		"256 bytes",			/* 3 */
		"512 bytes",			/* 4 */
		"1 KB",				/* 5 */
		"2 KB",				/* 6 */
		"4 KB",				/* 7 */
		"8 KB",				/* 8 */
		"16 KB",			/* 9 */
		"32 KB",			/* 10 */
		"64 KB",			/* 11 */
		"128 KB",			/* 12 */
		"256 KB",			/* 13 */
		"512 KB",			/* 14 */
		"1 MB",				/* 15 */
		"2 MB",				/* 16 */
		"4 MB",				/* 17 */
		"8 MB",				/* 18 */
		"16 MB",			/* 19 */
		"32 MB",			/* 20 */
		"64 MB",			/* 21 */
		"128 MB",			/* 22 */
		"256 MB",			/* 23 */
		"512 MB",			/* 24 */
		"1 GB",				/* 25 */
	};
	
	kprintf("HEAP DUMP\n");
	
	int bucket;
	for (bucket=0; bucket<NUM_BUCKETS; bucket++)
	{
		FreeSlab *slab;
		for (slab=buckets[bucket]; slab!=NULL; slab=slab->next)
		{
			kprintf("%p in bucket %d (%s)\n", slab, bucket, bucketLabels[bucket]);
		};
	};
};

#endif	/* __CONFIG_HEAP_SLAB */
