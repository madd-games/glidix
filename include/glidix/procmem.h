/*
	Glidix kernel

	Copyright (c) 2014-2015, Madd Games.
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

/**
 * Manipulating process memory.
 */

#include <glidix/pagetab.h>
#include <stdint.h>
#include <stddef.h>
#include <glidix/common.h>
#include <glidix/spinlock.h>
//#include <glidix/cowcache.h>

#define	PROT_READ			(1 << 0)
#define	PROT_WRITE			(1 << 1)
#define	PROT_EXEC			(1 << 2)
#define	PROT_ALLOC			(1 << 3)

#define	PROT_ALL			((1 << 4)-1)

#define	MEM_SEGMENT_COLLISION		-1
#define	MEM_SEGMENT_INVALID		-2

#define	MEM_MAKE			(1 << 0)

#define	FL_SHARED			(1 << 0)

#define	MAP_PRIVATE			(1 << 0)
#define	MAP_SHARED			(1 << 1)

typedef enum
{
	MEM_CURRENT = 0,
	MEM_OTHER = 1
} MemorySelector;

typedef struct
{
	/**
	 * Pages still not copied from this COW list.
	 * (sum of all refcounts).
	 */
	uint64_t pagesToGo;

	/**
	 * The frames.
	 */
	uint64_t *frames;

	/**
	 * Refcounts for each frame.
	 */
	uint64_t *refcounts;

	/**
	 * Number of FrameLists using this list.
	 */
	uint64_t users;
	Spinlock lock;
} COWList;

typedef struct
{
	int refcount;
	int count;
	uint64_t *frames;
	off_t fileOffset;
	size_t fileSize;
	Spinlock lock;
	int flags;		/* FL_* */

	/**
	 * If this is a copy-on-write frame list.
	 */
	COWList *cowList;
} FrameList;

typedef struct _Segment
{
	/**
	 * Index of the page where this segment begins.
	 */
	uint64_t			start;

	/**
	 * The frame list.
	 */
	FrameList			*fl;

	/**
	 * The protection flags.
	 */
	int				flags;

	/**
	 * Next segment.
	 */
	struct _Segment			*next;
} Segment;

typedef struct
{
	/**
	 * The reference count.
	 */
	int refcount;

	/**
	 * The physical frame address of the PDPT for this 512GB block.
	 */
	uint64_t pdptPhysFrame;

	/**
	 * List of segments in this process space.
	 */
	Segment *firstSegment;

	/**
	 * List of PD and PT frames that are to be erased when this ProcMem gets
	 * deleted.
	 */
	uint64_t *framesToCleanUp;
	int numFramesToCleanUp;

	/**
	 * The lock.
	 */
	Spinlock lock;
} ProcMem;

FrameList *palloc_later(int count, off_t fileOffset, size_t fileSize);
FrameList *palloc(int count);
FrameList *pmap(uint64_t start, int count);
void pupref(FrameList *fl);
void pdownref(FrameList *fl);

/**
 * Make a duplicate of all the frames in a list.
 */
FrameList *pdup(FrameList *old);

ProcMem *CreateProcessMemory();
int AddSegment(ProcMem *pm, uint64_t start, FrameList *frames, int flags);
int DeleteSegment(ProcMem *pm, uint64_t start);
void SetProcessMemory(ProcMem *pm);
ProcMem* DuplicateProcessMemory(ProcMem *pm);

void UprefProcessMemory(ProcMem *pm);
void DownrefProcessMemory(ProcMem *pm);

/**
 * Try copy-on-writing some data. Returns 0 if it happened, -1 otherwise.
 */
int tryCopyOnWrite(uint64_t addr);

/**
 * Try loading-on-demand some data. Returns 0 if it happened, -1 otherwise.
 */
int tryLoadOnDemand(uint64_t addr);

/**
 * Dump the contents of a process memory, for debugging purposes. Also show an arrow to indicate
 * which segment an address is in.
 */
void dumpProcessMemory(ProcMem *pm, uint64_t checkAddr);

/**
 * Userspace.
 */
#ifndef _SYS_MMAN_H
int mprotect(uint64_t addr, uint64_t len, int prot);
#endif

#endif
