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

#ifndef __glidix_pageinfo_h
#define __glidix_pageinfo_h

/**
 * We use a 4-level global page tree (similar to the PML4) to hold information about each
 * frame of PHYSICAL memory. This is used by the virtual memory manager to keep track of
 * how many address spaces a certain frame was mapped into (so as to not release it by
 * accident), and also by file paging to use some frames for caching while allowing them
 * to be returned when an application needs memory and no more is free.
 *
 * All levels of the tree contain pointers to the next level, except the bottommost level.
 * The bottommost level contains the number of references in the low 32 bits, and the high
 * 32 bits contain the flags, described below.
 */

#include <glidix/common.h>

/**
 * Set if the page is used for caching, and should not be freed except by the memory exchange
 * protocol.
 */
#define	PI_CACHE				(1UL << 32)

/**
 * Set if at least 1 process modified the page before unmapping it (so it needs to be committed
 * to disk).
 */
#define	PI_DIRTY				(1UL << 33)

/**
 * Set if at least 1 process accessed the page before unmapping it (so the access time needs to
 * be updated).
 */
#define	PI_ACCESSED				(1UL << 34)

/**
 * Page information node.
 */
typedef union PageInfoNode_
{
	uint64_t				entries[512];
	union PageInfoNode_*			branches[512];
} PageInfoNode;

/**
 * Initialize the page info system.
 */
void piInit();

/**
 * Create a new page with the specified flags, and returns its frame number. This involves a call to
 * phmAllocFrame() to get a new frame, and then sets its use count to 1, and sets the desired flags.
 * Returns 0 on error (out of memory).
 */
uint64_t piNew(uint64_t flags);

/**
 * Increase the reference count of a page. The page must already exist, and it must not be possible for
 * it to be released before this call returns. A lock-free algorithm is involved.
 */
void piIncref(uint64_t frame);

/**
 * Decrease the reference count of a page. If the reference count reaches 0 as a result of this call, and
 * the page is not used for caching (PI_CACHE), it will be freed.
 */
void piDecref(uint64_t frame);

/**
 * Mark a page dirty. The caller must own a reference to the page. This is done atomically.
 */
void piMarkDirty(uint64_t frame);

/**
 * Mark a page accessed. The caller must own a reference to the page. This is done atomically.
 */
void piMarkAccessed(uint64_t frame);

/**
 * Determine if a page needs to be copied upon a write. If this returns false (0) it means that the page can
 * just be marked writeable because nobody else is using it and no copying is needed.
 */
int piNeedsCopyOnWrite(uint64_t frame);

/**
 * Uncache the specified page, declaring it is now useless except when mapped somewhere.
 */
void piUncache(uint64_t frame);

/**
 * Mark a page clean (not dirty), and return true if it was dirty.
 */
int piCheckFlush(uint64_t frame);

/**
 * Mark a page as cached, with a reference count of 0xFFFFFF. The page must not have been accessed before.
 * This is used for things like mapping video memory.
 */
void piStaticFrame(uint64_t frame);

#endif
