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

#ifdef __CONFIG_HEAP_BLOCK

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

static Mutex heapLock;

static PD *pdHeap;
static int nextHeapTable;

static HeapHeader *lowestFreeHeader;

void initMemoryPhase2()
{
	// TODO: xd (execute disable, not the stupid face), we'll look at that shit in a bit.
	mutexInit(&heapLock);
	
	PML4 *pml4 = getPML4();
	PDPT *pdpt = kxmalloc(sizeof(PDPT), MEM_PAGEALIGN);
	nextHeapTable = 1;
	memset(pdpt, 0, sizeof(PDPT));
	PD *pd = kxmalloc(sizeof(PD), MEM_PAGEALIGN);
	memset(pd, 0, sizeof(PD));
	pdpt->entries[0].present = 1;
	pdpt->entries[0].rw = 1;
	pdpt->entries[0].pdPhysAddr = VIRT_TO_FRAME(pd);

	// page table for the first 2MB.
	PT *pt = kxmalloc(sizeof(PT), MEM_PAGEALIGN);
	memset(pt, 0, sizeof(PT));
	pd->entries[0].present = 1;
	pd->entries[0].rw = 1;
	pd->entries[0].ptPhysAddr = VIRT_TO_FRAME(pt);
	pdHeap = pd;

	// map the first 2MB to physical addresses.
	int i;
	for (i=0; i<512; i++)
	{
		pt->entries[i].present = 1;
		pt->entries[i].rw = 1;
		pt->entries[i].framePhysAddr = phmAllocFrame();
	};

	// set it in the PML4 (so it maps from HEAP_BASE_ADDR up).
	pml4->entries[258].present = 1;
	pml4->entries[258].rw = 1;
	pml4->entries[258].pdptPhysAddr = VIRT_TO_FRAME(pdpt);

	refreshAddrSpace();

	// create one giant (2MB) block for the heap.
	HeapHeader *head = (HeapHeader*) HEAP_BASE_ADDR;
	head->magic = HEAP_HEADER_MAGIC;
	head->size = 0x200000 - sizeof(HeapHeader) - sizeof(HeapFooter);
	head->flags = 0;		// this block is free.

	HeapFooter *foot = (HeapFooter*) (HEAP_BASE_ADDR + 0x200000 - sizeof(HeapFooter));
	foot->magic = HEAP_FOOTER_MAGIC;
	foot->size = head->size;
	foot->flags = 0;

	lowestFreeHeader = head;
	readyForDynamic = 1;
};

static HeapHeader *heapHeaderFromFooter(HeapFooter *foot)
{
	uint64_t footerAddr = (uint64_t) foot;
	uint64_t headerAddr = footerAddr - foot->size - sizeof(HeapHeader);
	return (HeapHeader*) headerAddr;
};

static HeapFooter *heapFooterFromHeader(HeapHeader *head)
{
	uint64_t headerAddr = (uint64_t) head;
	uint64_t footerAddr = headerAddr + sizeof(HeapHeader) + head->size;
	return (HeapFooter*) footerAddr;
};

void expandHeap()
{
	// expand the heap by 1 table
	if (nextHeapTable == 512)
	{
		panic("ran out of heap memory (expanding beyond 1GB)");
	};

	uint64_t frame = phmAllocFrame();
	char tmp[0x1000];
	memset(tmp, 0, 0x1000);

	PT *pt = (PT*) tmp;
	
	int i;
	for (i=0; i<512; i++)
	{
		pt->entries[i].present = 1;
		pt->entries[i].rw = 1;
		pt->entries[i].framePhysAddr = phmAllocFrame();
	};
	
	frameWrite(frame, tmp);

	pdHeap->entries[nextHeapTable].present = 1;
	pdHeap->entries[nextHeapTable].rw = 1;
	pdHeap->entries[nextHeapTable].ptPhysAddr = frame;
	nextHeapTable++;

	refreshAddrSpace();

	// address of the end of the heap before expansion
	uint64_t addr = HEAP_BASE_ADDR + (0x200000 * (nextHeapTable-1));

	// if the current last block is free, we'll extend it;
	// otherwise, we add a new 2MB block to fill this new space
	HeapFooter *lastFoot = (HeapFooter*) (addr - sizeof(HeapFooter));
	HeapHeader *lastHead = heapHeaderFromFooter(lastFoot);

	if (lastFoot->magic != HEAP_FOOTER_MAGIC)
	{
		panic("the last footer has invalid magic (located at %a)", lastFoot);
	};

	if (lastHead->flags & HEAP_BLOCK_TAKEN)
	{
		// taken, so make a new block, and tell the previous-last footer that
		// it now has a friend on the right.
		lastFoot->flags |= HEAP_BLOCK_HAS_RIGHT;

		HeapHeader *head = (HeapHeader*) addr;
		head->magic = HEAP_HEADER_MAGIC;
		head->size = 0x200000 - sizeof(HeapHeader) - sizeof(HeapFooter);
		head->flags = HEAP_BLOCK_HAS_LEFT;

		HeapFooter *foot = (HeapFooter*) (addr + head->size + sizeof(HeapHeader));
		foot->magic = HEAP_FOOTER_MAGIC;
		foot->size = head->size;
		foot->flags = 0;
	}
	else
	{
		// not taken, so expand
		lastHead->size += 0x200000;

		HeapFooter *foot = (HeapFooter*) (addr + 0x200000 - sizeof(HeapFooter));
		foot->magic = HEAP_FOOTER_MAGIC;
		foot->size = lastHead->size;
		foot->flags = 0;
	};
};

static HeapHeader *heapWalkRight(HeapHeader *head)
{
	uint64_t addr = (uint64_t) head;
	addr += sizeof(HeapHeader) + head->size + sizeof(HeapFooter);

	HeapFooter *foot = (HeapFooter*) (addr - sizeof(HeapFooter));
	if ((foot->flags & HEAP_BLOCK_HAS_RIGHT) == 0)
	{
		return NULL;
	};

	HeapHeader *nextHead = (HeapHeader*) addr;
	if (nextHead->magic != HEAP_HEADER_MAGIC)
	{
		//heapDump();
		stackTraceHere();
		panic("detected heap corruption (invalid header magic at %a, stepping from %a)", nextHead, head);
	};

	return nextHead;
};

static void heapSplitBlock(HeapHeader *head, size_t size)
{
	// find the current footer
	uint64_t currentHeaderAddr = (uint64_t) head;
	uint64_t currentFooterAddr = currentHeaderAddr + sizeof(HeapHeader) + head->size;
	HeapFooter *currentFooter = (HeapFooter*) currentFooterAddr;

	// change the size of the current header
	head->size = size;

	// decrease the size on the footer (we'll place the appropriate header there in a sec..)
	currentFooter->size -= (size + sizeof(HeapHeader) + sizeof(HeapFooter));

	// get the address of the new footer and header
	uint64_t newFooterAddr = currentHeaderAddr + sizeof(HeapHeader) + size;
	uint64_t newHeaderAddr = newFooterAddr + sizeof(HeapFooter);
	HeapFooter *newFooter = (HeapFooter*) newFooterAddr;
	HeapHeader *newHeader = (HeapHeader*) newHeaderAddr;

	// make the new header
	newHeader->magic = HEAP_HEADER_MAGIC;
	newHeader->flags = HEAP_BLOCK_HAS_LEFT;
	newHeader->size = currentFooter->size;

	// make the new footer
	newFooter->magic = HEAP_FOOTER_MAGIC;
	newFooter->flags = HEAP_BLOCK_HAS_RIGHT;
	newFooter->size = size;
};

void *kxmallocDynamic(size_t size, int flags, const char *aid, int lineno)
{
	// TODO: don't ignore the flags!
	void *retAddr = NULL;
	mutexLock(&heapLock);

	// make the size divisible by 16.
	if ((size & 0xF) != 0)
	{
		size = (size & ~0xF) + 16;
	};

	// find the first free block.
	HeapHeader *head = (HeapHeader*) HEAP_BASE_ADDR /*lowestFreeHeader*/;
	while ((head->flags & HEAP_BLOCK_TAKEN) || (head->size < size))
	{	
		HeapHeader *nextHead = heapWalkRight(head);
		if (nextHead == NULL)
		{
			if (head->flags & HEAP_BLOCK_TAKEN)
			{
				expandHeap();
				head = heapWalkRight(head);
			};

			while (head->size < size)
			{
				expandHeap();
			};
		}
		else
		{
			head = nextHead;
		};
	};

	if (head->size > (size+sizeof(HeapHeader)+sizeof(HeapFooter)+8))
	{
		heapSplitBlock(head, size);
	};

	retAddr = &head[1];		// the memory right after the header is the free block.
	head->flags |= HEAP_BLOCK_TAKEN;
	head->aid = aid;
	head->lineno = lineno;

	HeapFooter *foot = heapFooterFromHeader(head);
	foot->flags |= HEAP_BLOCK_TAKEN;

	mutexUnlock(&heapLock);
	return retAddr;
};

void *krealloc(void *block, size_t size)
{
	if (block == NULL) return kmalloc(size);

	uint64_t addr = (uint64_t)block - sizeof(HeapHeader);
	HeapHeader *head = (HeapHeader*) addr;

	if (head->magic != HEAP_HEADER_MAGIC)
	{
		panic("heap corruption detected: block header at %a has invalid magic", addr);
	};

	void *ret = kmalloc(size);
	if (size < head->size)
	{
		memcpy(ret, block, size);
	}
	else
	{
		memcpy(ret, block, head->size);
	};

	kfree(block);
	return ret;
};

void _kfree(void *block, const char *who, int line)
{
	// kfree()ing NULL is perfectly acceptable.
	if (block == NULL) return;
	
	mutexLock(&heapLock);

	// all blocks are at least 16 bytes in size because of the alignment magic; so we can safely
	// feed the generator
	feedRandom(*((uint64_t*)block));
	
	uint64_t addr = (uint64_t) block;
	if (addr < (HEAP_BASE_ADDR + sizeof(HeapHeader)))
	{
		panic("%s:%d: invalid pointer passed to kfree(): %p: below heap start", who, line, (void*)addr);
	};

	HeapHeader *head = (HeapHeader*) (addr - sizeof(HeapHeader));
	if (head->magic != HEAP_HEADER_MAGIC)
	{
		panic("%s:%d: invalid pointer passed to kfree(): %p: lacking or corrupt block header", who, line, (void*)addr);
	};

	HeapFooter *foot = heapFooterFromHeader(head);
	if (foot->magic != HEAP_FOOTER_MAGIC)
	{
		heapDump();
		stackTraceHere();
		panic("%s:%d: heap corruption detected: the header for %p is not linked to a valid footer", who, line, (void*)addr);
	};

	if ((head->flags & HEAP_BLOCK_TAKEN) == 0)
	{
		//heapDump();
		stackTraceHere();
		panic("%s:%d: invalid pointer passed to kfree(): %p: already free (this thread=%s)", who, line, (void*)addr,
			getCurrentThread()->name);
	};

	if (foot->size != head->size)
	{
		//heapDump();
		stackTraceHere();
		panic("%s:%d: heap corruption detected: the header for %p does not agree with the footer on block size", who, line, (void*)addr);
	};

	// try to join with adjacent blocks
	HeapHeader *headLeft = NULL;
	HeapFooter *footRight = NULL;

	if (head->flags & HEAP_BLOCK_HAS_LEFT)
	{
		HeapFooter *footLeft = (HeapFooter*) (addr - sizeof(HeapHeader) - sizeof(HeapFooter));
		if (footLeft->magic != HEAP_FOOTER_MAGIC)
		{
			//kprintf("ALLOCATED BY: %s\n", heapHeaderFromFooter(footLeft)->aid);
			//heapDump();
			stackTraceHere();
			panic("%s:%d: heap corruption detected: block to the left of %p is marked as existing but has invalid footer magic (0x%016lx, size=%d)", who, line, (void*)addr, footLeft->magic, (int)footLeft->size);
		};

		HeapHeader *tmpHead = heapHeaderFromFooter(footLeft);
		if ((tmpHead->flags & HEAP_BLOCK_TAKEN) == 0)
		{
			headLeft = tmpHead;
		};
	};

	if (foot->flags & HEAP_BLOCK_HAS_RIGHT)
	{
		HeapHeader *headRight = (HeapHeader*) &foot[1];
		if (headRight->magic != HEAP_HEADER_MAGIC)
		{
			panic("%s:%d: heap corruption detected: block to the right of %p is marked as existing but has invalid header magic", who, line, (void*)addr);
		};

		HeapFooter *tmpFoot = heapFooterFromHeader(headRight);
		if ((headRight->flags & HEAP_BLOCK_TAKEN) == 0)
		{
			footRight = tmpFoot;
		};
	};

	// monitoring
	if (head->flags & HEAP_BLOCK_MONITOR)
	{
		kprintf("kfree() called on a monitored block (%p)\n", (void*)block);
		stackTraceHere();
	};
	
	// mark this block as free
	head->flags &= ~HEAP_BLOCK_TAKEN;
	foot->flags &= ~HEAP_BLOCK_TAKEN;
	
	if ((headLeft != NULL) && (footRight == NULL))
	{
		// only join with the left block
		size_t newSize = headLeft->size + sizeof(HeapHeader) + sizeof(HeapFooter) + head->size;
		headLeft->size = newSize;
		foot->size = newSize;
		if (headLeft < lowestFreeHeader) lowestFreeHeader = headLeft;
	}
	else if ((headLeft == NULL) && (footRight != NULL))
	{
		// only join with the right block
		size_t newSize = head->size + sizeof(HeapHeader) + sizeof(HeapFooter) + footRight->size;
		head->size = newSize;
		footRight->size = newSize;
		if (head < lowestFreeHeader) lowestFreeHeader = head;
	}
	else if ((headLeft != NULL) && (footRight != NULL))
	{
		// join with both blocks (ie. join the left and right together, the current head/foot become
		// part of a block).
		size_t newSize = headLeft->size + footRight->size + head->size + 2*sizeof(HeapHeader) + 2*sizeof(HeapFooter);
		headLeft->size = newSize;
		footRight->size = newSize;
		if (head < lowestFreeHeader) lowestFreeHeader = head;
	}
	else
	{
		if (head < lowestFreeHeader) lowestFreeHeader = head;
	};

	mutexUnlock(&heapLock);
	//ASM("sti");
};

void heapDump()
{
	// dump the list of blocks to the console.
	uint64_t heapsz = 0;
	HeapHeader *head = (HeapHeader*) HEAP_BASE_ADDR;
	kprintf("---\n");
	kprintf("ADDR                   STAT     SIZE\n");
	while (1)
	{
		uint64_t addr = (uint64_t) &head[1];
		HeapFooter *foot = heapFooterFromHeader(head);
		if (1)
		{
			kprintf("%p     ", (void*)addr);
			const char *stat = "FREE";
			if (head->flags & HEAP_BLOCK_TAKEN)
			{
				stat = "USED";
			};
			kprintf(stat);
			kprintf("     %lu", head->size);

			kprintf(" ");
			if (head->magic != HEAP_HEADER_MAGIC)
			{
				kprintf("H");
			};
			if (foot->magic != HEAP_FOOTER_MAGIC)
			{
				kprintf("F");
			};

			if (head->flags & HEAP_BLOCK_TAKEN)
			{
				kprintf(" [%s:%d]", head->aid, head->lineno);
			};
			kprintf("\n");
		};

		heapsz += head->size + sizeof(HeapHeader) + sizeof(HeapFooter);
		if ((foot->flags & HEAP_BLOCK_HAS_RIGHT) || (foot->magic != HEAP_FOOTER_MAGIC))
		{
			head = (HeapHeader*) &foot[1];
		}
		else
		{
			break;
		};
	};

	uint64_t heapszMB = heapsz / 1024 / 1024;
	uint64_t heapszPercent = heapsz * 100 / 0x40000000;
	kprintf("Total heap usage: %lu/1024MB (%lu%%)\n", heapszMB, heapszPercent);
	kprintf("Lowest free header: %p, so lowest addr: %p\n", lowestFreeHeader, &lowestFreeHeader[1]);
	kprintf("---\n");
};

void checkBlockValidity(void *addr_)
{
	uint64_t addr = (uint64_t) addr_;
	
	HeapHeader *head = (HeapHeader*) (addr - sizeof(HeapHeader));
	if (head->magic != HEAP_HEADER_MAGIC)
	{
		panic("Block at %p broken: invalid header magic!", (void*)addr);
	};
	
	HeapFooter *foot = heapFooterFromHeader(head);
	if (foot->magic != HEAP_FOOTER_MAGIC)
	{
		panic("Block at %p broken: invalid footer magic!", (void*)addr);
	};
	
	if (head->size != foot->size)
	{
		panic("Block at %p broken: header claims size is %d, footer claims %d", (void*)addr, (int)head->size, (int)foot->size);
	};
	
	kprintf("[DEBUG] Block %p OK\n", (void*)addr);
};

void monitorBlock(void *addr_)
{
	uint64_t addr = (uint64_t) addr_;

	mutexLock(&heapLock);
	HeapHeader *head = (HeapHeader*) (addr - sizeof(HeapHeader));
	if (head->magic != HEAP_HEADER_MAGIC)
	{
		panic("Block at %p broken: invalid header magic!", (void*)addr);
	};
	
	HeapFooter *foot = heapFooterFromHeader(head);
	if (foot->magic != HEAP_FOOTER_MAGIC)
	{
		panic("Block at %p broken: invalid footer magic!", (void*)addr);
	};
	
	if (head->size != foot->size)
	{
		panic("Block at %p broken: header claims size is %d, footer claims %d", (void*)addr, (int)head->size, (int)foot->size);
	};

	head->flags |= HEAP_BLOCK_MONITOR;	
	mutexUnlock(&heapLock);
};

#endif	/* __CONFIG_HEAP_BLOCK */
