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
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>

pthread_mutex_t __heap_lock;

/**
 * Header attached to free slabs to form a linked list.
 */
struct free_slab
{
	struct free_slab *next;
};

/**
 * Header attached to used slabs.
 */
struct used_slab
{
	/**
	 * Bucket from which the slab came.
	 */
	uint64_t bucket;
	
	/**
	 * Real size of the block, as passed to malloc().
	 */
	uint64_t realSize;
	
	/**
	 * Slab data.
	 */
	char data[];
};

/**
 * Table of buckets. Each bucket is a linked list of free slabs of a specific size. For index n, the size is given by:
 *	slabSize = 2^(n+5)
 */
static struct free_slab* buckets[_HEAP_NUM_BUCKETS];

/**
 * Allocate a slab of the given bucket. If the bucket is out of slabs, take a slab from the next bucket and split it
 * in half. If there are no slabs left at all, return NULL.
 */
static struct free_slab* slab_alloc(int bucket)
{
	if (bucket >= _HEAP_NUM_BUCKETS) return NULL;
	size_t slabSize = (1UL << (5+bucket));
	
	if (buckets[bucket] != NULL)
	{
		struct free_slab *slab = buckets[bucket];
		buckets[bucket] = slab->next;
		return slab;
	}
	else
	{
		struct free_slab *up = slab_alloc(bucket+1);
		if (up == NULL) return NULL;
		
		struct free_slab *other = (struct free_slab*) ((uint64_t) up + slabSize);
		other->next = NULL;
		buckets[bucket] = other;
		return up;
	};
};

/**
 * Initialize the heap by mapping a single big slab.
 */
void _heap_init()
{
	size_t heapSize = 1UL << (_HEAP_NUM_BUCKETS + 5 - 1);
	void *heap = mmap(NULL, heapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (heap == MAP_FAILED)
	{
		raise(SIGABRT);
	};
	
	/* next is already set to NULL because mmap() returns zeroed-out blocks */
	buckets[_HEAP_NUM_BUCKETS-1] = (struct free_slab*) heap;
};

void *_heap_malloc(size_t len)
{
	if (len == 0)
	{
		errno = ENOMEM;
		return NULL;
	};
	
	// make space for the header
	len += 16;
	
	int bucket;
	for (bucket=0; bucket<_HEAP_NUM_BUCKETS; bucket++)
	{
		size_t bucketSize = 1UL << (bucket+5);
		if (bucketSize >= len) break;
	};
	
	if (bucket == _HEAP_NUM_BUCKETS)
	{
		errno = ENOMEM;
		return NULL;
	};
	
	struct free_slab *fslab = slab_alloc(bucket);
	if (fslab == NULL)
	{
		errno = ENOMEM;
		return NULL;
	};
	
	struct used_slab *uslab = (struct used_slab*) fslab;
	uslab->bucket = bucket;
	uslab->realSize = len-16;
	
	return uslab->data;
};

void *_heap_realloc(void *block, size_t newsize)
{
	if (newsize == 0)
	{
		_heap_free(block);
		errno = ENOMEM;
		return NULL;
	};
	
	if (block == NULL)
	{
		return _heap_malloc(newsize);
	};
	
	struct used_slab *uslab = (struct used_slab*) block;
	uslab--;
	
	// make space for the header
	newsize += 16;
	
	// check if we can split block in half
	if (uslab->bucket != 0)
	{
		size_t prevSize = 1UL << (uslab->bucket-1+5);
		if (newsize <= prevSize)
		{
			uslab->bucket--;
			struct free_slab *fslab = (struct free_slab*) ((uint64_t) uslab + prevSize);
			fslab->next = buckets[uslab->bucket];
			buckets[uslab->bucket] = fslab;
			
			uslab->realSize = newsize-16;
			return block;
		};
	};
	
	// check if we are still within bounds of the current block
	size_t currentSize = 1UL << (uslab->bucket+5);
	if (newsize <= currentSize)
	{
		uslab->realSize = newsize-16;
		return block;
	};
	
	// we must reallocate
	void *newblock = _heap_malloc(newsize-16);
	if (newblock == NULL)
	{
		errno = ENOMEM;
		return NULL;
	};
	
	size_t toCopy = uslab->realSize;
	if (toCopy > newsize-16) toCopy = newsize-16;
	
	memcpy(newblock, block, toCopy);
	_heap_free(block);
	return newblock;
};

void _heap_free(void *block)
{
	if (block == NULL) return;
	
	struct used_slab *uslab = (struct used_slab*) block;
	uslab--;
	
	int bucket = (int) uslab->bucket;
	struct free_slab *fslab = (struct free_slab*) uslab;
	
	fslab->next = buckets[bucket];
	buckets[bucket] = fslab;
};
