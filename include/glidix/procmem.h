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
#include <glidix/common.h>
#include <glidix/spinlock.h>

#define	PROT_READ			(1 << 0)
#define	PROT_WRITE			(1 << 1)
#define	PROT_EXEC			(1 << 2)
#define	PROT_ALLOC			(1 << 3)

#define	PROT_ALL			((1 << 4)-1)

#define	MEM_SEGMENT_COLLISION		-1
#define	MEM_SEGMENT_INVALID		-2

typedef struct
{
	int refcount;
	int count;
	uint64_t *frames;
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

FrameList *palloc(int count);
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
 * Userspace.
 */
int mprotect(uint64_t addr, uint64_t len, int prot);

#endif
