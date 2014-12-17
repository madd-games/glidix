/*
	Glidix Runtime

	Copyright (c) 2014, Madd Games.
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

/**
 * WARNING: Do not implement locks in this code! Instead, go to the definitions of malloc(),
 * realloc(), calloc(), free(), etc. and implement them there!
 */

static uint64_t heapTop;

void _heap_init()
{
	if (mprotect((void*)_HEAP_BASE_ADDR, _HEAP_PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_ALLOC) != 0)
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
	uint64_t addr = (uint64_t) head + head->size;
	return (__heap_footer*) addr;
};

__heap_header *_heap_get_header(__heap_footer *foot)
{
	uint64_t addr = (uint64_t) foot - foot->size - sizeof(__heap_header);
	return (__heap_header*) addr;
};

void _heap_split_block(__heap_header *head, size_t newSize)
{
	if (head->size < (sizeof(__heap_header)+sizeof(__heap_footer)+8))
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

	foot->size -= (newSize + sizeof(__heap_header) + sizeof(__heap_footer));
	__heap_header *newHead = _heap_get_header(foot);
	newHead->magic = _HEAP_HEADER_MAGIC;
	newHead->size = foot->size;
	newHead->flags = _HEAP_BLOCK_HAS_LEFT;
};

void* _heap_malloc(size_t len)
{
	__heap_header *head = (__heap_header*) _HEAP_BASE_ADDR;
	while ((head->size < len) || (head->flags & _HEAP_BLOCK_USED))
	{
		__heap_footer *foot = _heap_get_footer(head);
		if ((foot->flags & _HEAP_BLOCK_HAS_RIGHT) == 0)
		{
			// TODO: expand heap
			return NULL;
		};

		head = (__heap_header*) &foot[1];
	};

	_heap_split_block(head, len);
	return (void*) &head[1];
};

void _heap_free(void *block)
{
	/* TODO: check if this block is legit */
	/* TODO: block unification */
	uint64_t addr = (uint64_t) block - sizeof(__heap_header);
	__heap_header *header = (__heap_header*) addr;
	header->flags &= ~_HEAP_BLOCK_USED;
};