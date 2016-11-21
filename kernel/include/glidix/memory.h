/*
	Glidix kernel

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

#ifndef __glidix_memory_h
#define __glidix_memory_h

#include <glidix/common.h>
#include <stddef.h>

/**
 * Routines for dynamic memory allocation and deallocation.
 */

#define	MEM_PAGEALIGN			1

void initMemoryPhase1(uint64_t placement, uint64_t size);
void initMemoryPhase2();
void *_kxmalloc(size_t size, int flags, const char *aid, int lineno);
void *_kmalloc(size_t size, const char *aid, int lineno);
void *krealloc(void *block, size_t size);
void _kfree(void *block, const char *who, int line);
void heapDump();

#define	kmalloc(size)			_kmalloc((size), __FILE__, __LINE__)
#define	kxmalloc(size, flags)		_kxmalloc((size), (flags), "", 0)
#define	kfree(x)			_kfree((x), __FILE__, __LINE__)

/**
 * Private heap structures.
 */

#define	HEAP_HEADER_MAGIC		0xDEADBEEF
#define	HEAP_FOOTER_MAGIC		0xBAD00BAD

// flags
#define	HEAP_BLOCK_TAKEN		1		// shared flag
#define	HEAP_BLOCK_HAS_LEFT		2		// header flag
#define	HEAP_BLOCK_HAS_RIGHT		4		// footer flag
#define	HEAP_BLOCK_MONITOR		8		// header flag (for debugging)

typedef struct
{
	uint32_t magic;
	uint64_t size;
	uint8_t  flags;
	const char *aid;				// allocator string (filename and lineno of kmalloc(), for debugging)
	int lineno;
	uint8_t pad[7];					// make the size divisible by 16.
} PACKED HeapHeader;

typedef struct
{
	uint32_t magic;
	uint64_t size;
	uint8_t  flags;
	uint8_t  pad[3];				// make the size divisible by 16.
} PACKED HeapFooter;

/**
 * Check if the specified heap block is valid (not broken). Used for debugging purposes. Prints "[DEBUG] Block (addr) OK"
 * if the block is still valid, or a panic if not.
 */
void checkBlockValidity(void *addr);

/**
 * Mark a block for monitoring. The kernel will print stack traces whenever it is freed.
 */
void monitorBlock(void *addr);

#endif
