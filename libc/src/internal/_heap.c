/*
	Glidix Runtime

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

#include <_heap.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/debug.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define	HEAP_DEBUG_GUARD_PAGES			10

pthread_mutex_t __heap_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * WARNING: Do not implement locks in this code! Instead, go to the definitions of malloc(),
 * realloc(), calloc(), free(), etc. and implement them there!
 */

static __heap_header *firstHead;
static int heapDebugMode = 0;

void _heap_init()
{
	void *addr = mmap(NULL, _HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (addr == MAP_FAILED) abort();
	
	// create the initial heap block
	__heap_header *head = (__heap_header*) addr;
	__heap_footer *foot = (__heap_footer*) ((uint64_t)addr + _HEAP_SIZE - sizeof(__heap_footer));

	head->magic = _HEAP_HEADER_MAGIC;
	head->size = _HEAP_SIZE - sizeof(__heap_header) - sizeof(__heap_footer);
	head->flags = 0;

	foot->magic = _HEAP_FOOTER_MAGIC;
	foot->size = head->size;
	foot->flags = 0;
	
	firstHead = head;
};

__heap_footer *_heap_get_footer(__heap_header *head)
{
	uint64_t addr = (uint64_t) head + head->size + sizeof(__heap_header);
	return (__heap_footer*) addr;
};

__heap_header *_heap_get_header(__heap_footer *foot)
{
	uint64_t addr = (uint64_t) foot - foot->size - (size_t)sizeof(__heap_header);
	return (__heap_header*) addr;
};

void _heap_split_block(__heap_header *head, size_t newSize)
{
	if (head->size < (newSize+sizeof(__heap_header)+sizeof(__heap_footer)+16))
	{
		// don't split blocks below 16 bytes, there'll be enough heap fragmentation from
		// malloc() calls below 16 bytes.
		return;
	};

	__heap_footer *foot = _heap_get_footer(head);
	head->size = newSize;

	__heap_footer *newFoot = _heap_get_footer(head);
	newFoot->magic = _HEAP_FOOTER_MAGIC;
	newFoot->size = newSize;
	newFoot->flags = _HEAP_BLOCK_HAS_RIGHT;

	foot->size -= (newSize + (size_t)sizeof(__heap_header) + (size_t)sizeof(__heap_footer));
	__heap_header *newHead = _heap_get_header(foot);
	if (newHead < firstHead)
	{
		fprintf(stderr, "libc: splitting block at %p, size %u, gives a header %p, below %p (below heap)\n", head, (unsigned int)newSize, newHead, firstHead);
		abort();
	};
	newHead->magic = _HEAP_HEADER_MAGIC;
	newHead->size = foot->size;
	newHead->flags = _HEAP_BLOCK_HAS_LEFT;
};

void* _heap_malloc(size_t len)
{
	if (len == 0) return NULL;
	size_t requestedLen = len;
	len = (len + 0xF) & ~0xF;
	
	if (heapDebugMode)
	{
		len = (len + 0xFFF) & ~0xFFF;
		size_t totalSize = len + 0x1000 * 2 * HEAP_DEBUG_GUARD_PAGES;
		void *ptr = mmap(NULL, totalSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
		if (ptr == MAP_FAILED)
		{
			return NULL;
		};
		
		char *buf = (char*) ptr;
		*((uint64_t*)buf) = requestedLen;
		*((uint64_t*)(buf+8)) = len;
		char *guard = &buf[0x1000];
		munmap(guard, 0x1000 * HEAP_DEBUG_GUARD_PAGES - 0x1000);
		char *blockStart = &buf[0x1000 * HEAP_DEBUG_GUARD_PAGES];
		char *blockEnd = &blockStart[len];
		
		munmap(blockEnd, 0x1000 * HEAP_DEBUG_GUARD_PAGES);
		return blockStart;
	};
	
	__heap_header *head = firstHead;
	while ((head->size < len) || (head->flags & _HEAP_BLOCK_USED))
	{
		if (head->magic != _HEAP_HEADER_MAGIC)
		{
			fprintf(stderr, "heap corruption detected: header at %p has invalid magic\n", head);
			_heap_dump();
			abort();
		};

		__heap_footer *foot = _heap_get_footer(head);
		if (foot->magic != _HEAP_FOOTER_MAGIC)
		{
			fprintf(stderr, "heap corruption detected: footer for header at %p has invalid magic\n", head);
			_heap_dump();
			abort();
		};

		if ((foot->flags & _HEAP_BLOCK_HAS_RIGHT) == 0)
		{
			return NULL;
		};

		head = (__heap_header*) &foot[1];
	};

	if (head->magic != _HEAP_HEADER_MAGIC)
	{
		_heap_dump();
		fprintf(stderr, "libc: invalid header magic at %p\n", head);
		abort();
		_exit(1);
	};
	
	head->flags |= _HEAP_BLOCK_USED;
	_heap_split_block(head, len);
	return (void*) &head[1];
};

void _heap_free(void *block);
void *_heap_realloc(void *block, size_t newsize)
{
	if (block == NULL) return _heap_malloc(newsize);
	size_t oldsize;
	
	if (heapDebugMode)
	{
		uint64_t addr = (uint64_t) block - 0x1000 * HEAP_DEBUG_GUARD_PAGES;
		oldsize = *((uint64_t*)addr);
	}
	else
	{
		uint64_t addr = (uint64_t) block - sizeof(__heap_header);
		__heap_header *header = (__heap_header*) addr;
		oldsize = header->size;
	};
	
	void *newblock = _heap_malloc(newsize);
	if (newsize > oldsize)
	{
		memcpy(newblock, block, oldsize);
	}
	else
	{
		memcpy(newblock, block, newsize);
	};

	_heap_free(block);
	return newblock;
};

void _heap_free(void *block)
{
	if (block == NULL) return;
	
	if (heapDebugMode)
	{
#if 0
		printf("__heap_free__\n");
		uint64_t addr = (uint64_t) block - 0x1000 * HEAP_DEBUG_GUARD_PAGES;
		printf("__free_A\n");
		uint64_t mainLen = *((uint64_t*)(addr+8));
		printf("__free_B (addr=%p, mainLen=%lu)\n", (void*)addr, mainLen);
		mprotect((void*)addr, 0x1000, PROT_NONE);
		printf("__free_C (block=%p, mainLen=%lu)\n", block, mainLen);
		mprotect(block, mainLen, PROT_NONE);
		printf("__free_end__\n");
#endif
		return;
	};
	
	uint64_t addr = (uint64_t) block - sizeof(__heap_header);
	__heap_header *header = (__heap_header*) addr;
	if (addr < (uint64_t)firstHead)
	{
		fprintf(stderr, "libc: invalid pointer passed to free(): %p\n", block);
		_heap_dump();
		abort();
	};

	if (header->magic != _HEAP_HEADER_MAGIC)
	{
		fprintf(stderr, "libc: heap corruption detected! header at %p is invalid\n", header);
		_heap_dump();
		abort();
	};

	header->flags &= ~_HEAP_BLOCK_USED;

	__heap_header *headerLeft = header;
	__heap_footer *footerRight = _heap_get_footer(header);
	
	if (header->flags & _HEAP_BLOCK_HAS_LEFT)
	{
		uint64_t leftFootAddr = addr - sizeof(__heap_footer);
		__heap_footer *leftFoot = (__heap_footer*) leftFootAddr;
		
		uint64_t leftHeadAddr = leftFootAddr - leftFoot->size - sizeof(__heap_header);
		__heap_header *head = (__heap_header*)leftHeadAddr;
		
		if ((head->flags & _HEAP_BLOCK_USED) == 0)
		{
			headerLeft = head;
		};
	};
	
	__heap_footer *footer = _heap_get_footer(header);
	if (footer->flags & _HEAP_BLOCK_HAS_RIGHT)
	{
		uint64_t rightHeadAddr = (uint64_t)footer + sizeof(__heap_footer);
		__heap_header *rightHead = (__heap_header*) rightHeadAddr;
		
		__heap_footer *foot = _heap_get_footer(rightHead);
		if ((rightHead->flags & _HEAP_BLOCK_USED) == 0)
		{
			footerRight = foot;
		};
	};
	
	uint64_t addrLeft = (uint64_t) headerLeft;
	uint64_t addrRight = (uint64_t) footerRight;
	
	uint64_t newSize = addrRight - addrLeft - sizeof(__heap_header);
	headerLeft->size = newSize;
	footerRight->size = newSize;
};

void _heap_dump()
{
	__heap_header *head = firstHead;
	printf("libc: dumping the heap:\n");

	while (1)
	{
		if ((uint64_t)head > 0x7c0000f710UL)
		{
			break;
		};
		
		printf("%p ", &head[1]);
		if (head->flags & _HEAP_BLOCK_USED)
		{
			printf("USED ");
		}
		else
		{
			printf("FREE ");
		};

		if (head->magic == _HEAP_HEADER_MAGIC)
		{
			printf(" ");
		}
		else
		{
			printf("H");
		};

		__heap_footer *foot = _heap_get_footer(head);
		if (foot->magic == _HEAP_FOOTER_MAGIC)
		{
			printf(" ");
		}
		else
		{
			printf("F");
		};

		if (head->size != foot->size)
		{
			printf("S");
		}
		else
		{
			printf(" ");
		};

		printf("size=%lu, footsz=%lu\n", head->size, foot->size);
		if (foot->flags & _HEAP_BLOCK_HAS_RIGHT)
		{
			head = (__heap_header*) &foot[1];
		}
		else
		{
			break;
		};
	};

	printf("end of heap\n");
};
