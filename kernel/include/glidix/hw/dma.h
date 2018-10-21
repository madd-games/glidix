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

#ifndef __glidix_dma_h
#define __glidix_dma_h

#include <glidix/util/common.h>

/**
 * Flags for dmaCreateBuffer().
 */
#define	DMA_32BIT			(1 << 0)

/**
 * Errors returned by dmaCreateBuffer().
 */
#define	DMA_ERR_NOMEM			-1
#define	DMA_ERR_NOT32			-2
#define	DMA_ERR_NOPAGES			-3

/**
 * Describes a DMA buffer. Initialize this structure using dmaCreateBuffer(), then release
 * the memory using dmaReleaseBuffer(). Use dmaGetPhys() and dmaGetPtr() to get the physical
 * address of the buffer, and the virtual address by which to access it, respectively.
 */
typedef struct
{
	uint64_t			firstFrame;
	uint64_t			numFrames;
	uint64_t			firstPage;
} DMABuffer;

/**
 * Describes a region extracted by dmaFirstRegion() and dmaNextRegion().
 */
typedef struct
{
	/**
	 * Physical address of region.
	 */
	uint64_t			physAddr;
	
	/**
	 * Size of this region.
	 */
	uint64_t			physSize;
	
	/**
	 * Virtual address of remainder of buffer.
	 */
	uint64_t			virtNext;
	
	/**
	 * Size left in the buffer.
	 */
	size_t				remSize;
	
	/**
	 * Maximum size of a region, or 0 if no limit.
	 */
	size_t				maxRegion;
} DMARegion;

/**
 * Initialize the DMA subsystem.
 */
void dmaInit();

/**
 * Create a new DMA buffer, and fill the 'handle' with all required information to manipulate it.
 * Call this before doing anything else with the DMA buffer.
 * Flags:
 *	DMA_32BIT		Fail if not all physical addresses fit in 32 bits.
 *
 * Returns 0 on success, negative value on failure. Upon failure, the contents of 'handle' are undefined.
 */
int dmaCreateBuffer(DMABuffer *handle, size_t bufsize, int flags);

/**
 * Release a previously-allocated DMA buffer, invalidating the contents of 'handle', and freeing any
 * memory used by the buffer. Further accesses to the DMA buffer's virtual address, or the handle,
 * result in undefined behavior.
 */
void dmaReleaseBuffer(DMABuffer *handle);

/**
 * Return the physical address of the DMA buffer - this can be passed to DMA devices. If the DMA_32BIT
 * flag was passed to dmaCreateBuffer(), the returned value can be saely cast to a uint32_t.
 */
uint64_t dmaGetPhys(DMABuffer *handle);

/**
 * Return the pointer to virtual memory where the buffer contents can be accessed.
 */
void* dmaGetPtr(DMABuffer *handle);

/**
 * These functions convert virtual buffers into physical addresses. A call to dmaFirstRegion() returns the
 * first physically consecutive region in the buffer; following calls to dmaNextRegion() return the next
 * regions until, at the end, 'physAddr' is set to 0. Results are returned into the DMARegion structure.
 *
 * If 'maxRegion' is zero, then there is no limit on the size of the returned regions. Otherwise, a region
 * will have at most 'maxRegion' bytes.
 *
 * These functions will trigger a undefined behaviour if one or more pages of the buffer are not correctly mapped.
 * In other words, pass only trusted buffers, allocated by the kernel!
 */
void dmaFirstRegion(DMARegion *reg, const void *buffer, size_t bufsize, size_t maxRegion);
void dmaNextRegion(DMARegion *reg);

#endif
