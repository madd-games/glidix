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

#ifndef __glidix_buddy_h
#define __glidix_buddy_h

#include <glidix/util/common.h>

#define BUDDY_NUM_ORDERS					36
#define BUDDY_MAX_REGIONS					64

/**
 * A header attached to a free memory block.
 */
typedef struct FreeMemoryHeader
{
	struct FreeMemoryHeader*				prev;
	struct FreeMemoryHeader*				next;
} FreeMemoryHeader;

/**
 * Represents a bucket of memory blocks of a specific order.
 */
typedef struct
{
	uint64_t						bitmapOffset;		// in bits
	FreeMemoryHeader*					freeHead;
} MemoryBucket;

/**
 * Represents a region of memory in the buddy allocator.
 */
typedef struct
{
	uint64_t						base;
	uint64_t						numPages;
	uint8_t*						bitmap;
	MemoryBucket						buckets[BUDDY_NUM_ORDERS];
} MemoryRegion;

/**
 * Initialize the buddy allocator.
 */
void buddyInit();

#endif