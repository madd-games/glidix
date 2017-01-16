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

#ifndef __HEAP_H
#define __HEAP_H

#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Some stuff about the libc heap.
 */

#define	_HEAP_SIZE					0x4000000000	/* 256GB */
#define	_HEAP_HEADER_MAGIC				0xDEADBEEF
#define	_HEAP_FOOTER_MAGIC				0xBEEF2BAD

/* header flags */
#define	_HEAP_BLOCK_USED				(1 << 0)
#define	_HEAP_BLOCK_HAS_LEFT				(1 << 1)

/* footer flags */
#define	_HEAP_BLOCK_HAS_RIGHT				(1 << 0)

typedef struct
{
	size_t size;
	uint8_t flags;
	uint32_t magic;
} __heap_header;

typedef struct
{
	size_t size;
	uint8_t flags;
	uint32_t magic;
} __heap_footer;

void _heap_init();
__heap_footer* _heap_get_footer(__heap_header *head);
__heap_header* _heap_get_header(__heap_footer *foot);
void _heap_split_block(__heap_header *head, size_t newSize);
void *_heap_malloc(size_t len);
void *_heap_realloc(void *block, size_t newsize);
void _heap_free(void *block);
void _heap_dump();
void _heap_expand();

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif
