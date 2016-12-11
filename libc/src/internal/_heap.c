/*
	Glidix Runtime

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

#include <_heap.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t __heap_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * WARNING: Do not implement locks in this code! Instead, go to the definitions of malloc(),
 * realloc(), calloc(), free(), etc. and implement them there!
 */

static uint64_t heapTop;

void _heap_init()
{
#if 0
	if (mprotect((void*)_HEAP_BASE_ADDR, _HEAP_PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_ALLOC) != 0)
	{
		abort();
	};
#endif

	if (mmap((void*)_HEAP_BASE_ADDR, _HEAP_PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0) == MAP_FAILED)
	{
		abort();
	};
	
	// create the initial heap block
	__heap_header *head = (__heap_header*) _HEAP_BASE_ADDR;
	__heap_footer *foot = (__heap_footer*) (_HEAP_BASE_ADDR + _HEAP_PAGE_SIZE - sizeof(__heap_footer));

	head->magic = _HEAP_HEADER_MAGIC;
	head->size = _HEAP_PAGE_SIZE - sizeof(__heap_header) - sizeof(__heap_footer);
	head->flags = 0;

	foot->magic = _HEAP_FOOTER_MAGIC;
	foot->size = head->size;
	foot->flags = 0;

	heapTop = _HEAP_BASE_ADDR + _HEAP_PAGE_SIZE;
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
	if (head->size < (newSize+sizeof(__heap_header)+sizeof(__heap_footer)+8))
	{
		// don't split blocks below 8 bytes, there'll be enough heap fragmentation from
		// malloc() calls below 8 bytes.
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
	if (newHead < (__heap_header*)_HEAP_BASE_ADDR)
	{
		fprintf(stderr, "libc: splitting block at %p, size %u, gives a header %p, below %p (below heap)\n", head, (unsigned int)newSize, newHead, (void*)_HEAP_BASE_ADDR);
		abort();
	};
	newHead->magic = _HEAP_HEADER_MAGIC;
	newHead->size = foot->size;
	newHead->flags = _HEAP_BLOCK_HAS_LEFT;
};

static int __i_debug = 0;
void __i_am_debug()
{
	__i_debug = 1;
};

void* _heap_malloc(size_t len)
{
	if (len == 0) return NULL;

	if (len & 0xF)
	{
		len &= ~0xF;
		len += 0x10;
	};
	
	__heap_header *head = (__heap_header*) _HEAP_BASE_ADDR;
	while ((head->size < len) || (head->flags & _HEAP_BLOCK_USED))
	{
		if (head->magic != _HEAP_HEADER_MAGIC)
		{
			fprintf(stderr, "heap corruption detected: header at %p has invalid magic\n", head);
			abort();
		};

		__heap_footer *foot = _heap_get_footer(head);
		if ((foot->flags & _HEAP_BLOCK_HAS_RIGHT) == 0)
		{
			_heap_expand();
			if (head->flags & _HEAP_BLOCK_USED)
			{
				head = (__heap_header*) &foot[1];
				foot = _heap_get_footer(head);
			};

			while (head->size < len)
			{
				_heap_expand();
			};

			break;
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
	uint64_t addr = (uint64_t) block - sizeof(__heap_header);
	__heap_header *header = (__heap_header*) addr;
	void *newblock = _heap_malloc(newsize);
	if (newsize > header->size)
	{
		memcpy(newblock, block, header->size);
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

	uint64_t addr = (uint64_t) block - sizeof(__heap_header);
	__heap_header *header = (__heap_header*) addr;
	if (addr < _HEAP_BASE_ADDR)
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
	__heap_header *head = (__heap_header*) _HEAP_BASE_ADDR;
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

void _heap_expand()
{
	if (mmap((void*)heapTop, _HEAP_PAGE_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANON | MAP_FIXED, -1, 0) == MAP_FAILED)
	{
		fprintf(stderr, "libc: failed to mmap() a heap page at %p\n", (void*)heapTop);
		abort();
	};
	
	// get the last block
	__heap_footer *lastFoot = (__heap_footer*) (heapTop - sizeof(__heap_footer));
	__heap_header *lastHead = _heap_get_header(lastFoot);

	if (lastHead->flags & _HEAP_BLOCK_USED)
	{
		// the last block is used, so make a new one
		__heap_header *head = (__heap_header*) heapTop;
		head->magic = _HEAP_HEADER_MAGIC;
		head->size = _HEAP_PAGE_SIZE - sizeof(__heap_header) - sizeof(__heap_footer);
		head->flags = _HEAP_BLOCK_HAS_LEFT;

		__heap_footer *foot = _heap_get_footer(head);
		foot->magic = _HEAP_FOOTER_MAGIC;
		foot->size = head->size;
		foot->flags = 0;

		lastFoot->flags |= _HEAP_BLOCK_HAS_RIGHT;
	}
	else
	{
		// the last block is not used, so expand it
		__heap_header *head = lastHead;
		head->size += _HEAP_PAGE_SIZE;
		__heap_footer *foot = _heap_get_footer(head);
		foot->magic = _HEAP_FOOTER_MAGIC;
		foot->size = head->size;
		foot->flags = 0;
	};

	// make sure we expand heapTop!
	heapTop += _HEAP_PAGE_SIZE;
};
