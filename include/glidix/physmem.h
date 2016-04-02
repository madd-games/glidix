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

#ifndef __glidix_physmem_h
#define __glidix_physmem_h

#include <glidix/common.h>
#include <stdint.h>

void initPhysMem(uint64_t numPages, MultibootMemoryMap *mmap, uint64_t mmapEnd);
void initPhysMem2();

/**
 * Allocate a list of consecutive frames, and return the index of the first one.
 * Flags should be 0.
 * Return 0 on failure.
 */
uint64_t phmAllocFrameEx(uint64_t count, int flags);

/**
 * Finds any free frame, marks it as used, and returns the frame index. The return value shifted
 * left by 12 bits gives the physical address of the first byte of this frame.
 */
uint64_t phmAllocFrame();

/**
 * Calls phmAllocFrame() to allcoate a frame, then zeroes it out.
 */
uint64_t phmAllocZeroFrame();

/**
 * NOTE: Do not free frames until the heap is set up and initPhysMem2() was called.
 */
void phmFreeFrame(uint64_t frame);
void phmFreeFrameEx(uint64_t start, uint64_t count);

#endif
