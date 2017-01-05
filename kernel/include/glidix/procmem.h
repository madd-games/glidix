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

#ifndef __glidix_procmem_h
#define __glidix_procmem_h

#include <glidix/common.h>
#include <glidix/ftree.h>
#include <glidix/semaphore.h>

/**
 * Protection settings.
 */
#define	PROT_READ			(1 << 0)
#define	PROT_WRITE			(1 << 1)
#define	PROT_EXEC			(1 << 2)

#define	PROT_ALL			((1 << 4)-1)

/**
 * Only define those if they weren't yet defined, since we might have been included by
 * a userspace application with <sys/mman.h> already included.
 */
#ifndef MAP_FAILED
#	define	MAP_PRIVATE			(1 << 0)
#	define	MAP_SHARED			(1 << 1)
#	define	MAP_ANON			(1 << 2)
#	define	MAP_FIXED			(1 << 3)
#	define	MAP_THREAD			(1 << 4)
#	define	MAP_FAILED			((uint64_t)-1)
#endif

/**
 * Minimum allowed address for mapping.
 */
#define	ADDR_MIN				0x200000

/**
 * Describes a segment in a virtual address space.
 */
typedef struct Segment_
{
	/**
	 * Links to the previous and next segment.
	 */
	struct Segment_*			prev;
	struct Segment_*			next;
	
	/**
	 * Size of this segment, in pages.
	 */
	size_t					numPages;
	
	/**
	 * The tree being mapped.
	 */
	FileTree*				ft;
	
	/**
	 * Offset into the file (page-aligned).
	 */
	uint64_t				offset;
	
	/**
	 * The thread that created this process (ignored unless MAP_THREAD was passed).
	 */
	Thread*					creator;
	
	/**
	 * Mapping flags (MAP_*). If 0, then nothing is mapped here.
	 */
	int					flags;
	
	/**
	 * Default permissions if unchanged for specific pages.
	 */
	int					prot;
} Segment;

/**
 * Description of a virtual address space.
 */
typedef struct
{
	/**
	 * Lock for the object.
	 */
	Semaphore				lock;
	
	/**
	 * Head of the segment list.
	 */
	Segment*				segs;
	
	/**
	 * Reference count.
	 */
	int					refcount;
	
	/**
	 * Physical frame number of the PDPT.
	 */
	uint64_t				phys;
} ProcMem;

/**
 * Create a new blank address space and switch to it. Returns 0 on success, -1 on error.
 */
int vmNew();

/**
 * Establish a memory mapping. 'addr' and 'off' must both be page-aligned. The file 'fp' is
 * only used if MAP_ANON is not passed in 'flags'. If 'addr' is 0, then the highest possible
 * address is chosen such that the new segment does not collide with others.
 *
 * On success, returns an address larger than or equal to ADDR_MIN, otherwise an error number,
 * e.g. ENODEV.
 */
struct _File;
uint64_t vmMap(uint64_t addr, size_t len, int prot, int flags, struct _File *fp, off_t off);

#endif
