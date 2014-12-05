/*
	Glidix kernel

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

#ifndef __glidix_memory_h
#define __glidix_memory_h

#include <glidix/common.h>
#include <stddef.h>

/**
 * Routines for dynamic memory allocation and deallocation.
 */

#define	MEM_PAGEALIGN			1

void initMemoryPhase1();
void initMemoryPhase2();
void *kxmalloc(size_t size, int flags);
void *kmalloc(size_t size);
void kfree(void *block);
void heapDump();

/**
 * Private heap structures.
 */

#define	HEAP_HEADER_MAGIC		0xDEADBEEF
#define	HEAP_FOOTER_MAGIC		0xBAD00BAD

// flags
#define	HEAP_BLOCK_TAKEN		1		// shared flag
#define	HEAP_BLOCK_HAS_LEFT		2		// header flag
#define	HEAP_BLOCK_HAS_RIGHT		4		// footer flag

typedef struct
{
	uint32_t magic;
	uint64_t size;
	uint8_t  flags;
} PACKED HeapHeader;

typedef struct
{
	uint32_t magic;
	uint64_t size;
	uint8_t  flags;
} PACKED HeapFooter;

#endif
