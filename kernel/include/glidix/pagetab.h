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

#ifndef __glidix_pagetab_h
#define __glidix_pagetab_h

#include <glidix/common.h>
#include <stdint.h>

/**
 * Page fault flags (set by the CPU).
 */
#define	PF_PRESENT			(1 << 0)		/* page is present */
#define	PF_WRITE			(1 << 1)		/* a write was attempted (else a read) */
#define	PF_USER				(1 << 2)		/* caused by user-space code */
#define	PF_RESV				(1 << 3)		/* reserved bits were set in a page table */
#define	PF_FETCH			(1 << 4)		/* caused by instruction fetch */

typedef struct
{
	uint64_t			present:1;
	uint64_t			rw:1;
	uint64_t			user:1;
	uint64_t			pwt:1;
	uint64_t			pcd:1;
	uint64_t			accessed:1;
	uint64_t			ignored:1;
	uint64_t			ps:1;
	uint64_t			moreIgnored:4;
	uint64_t			pdptPhysAddr:36;
	uint64_t			zero:4;
	uint64_t			evenMoreIgnored:11;
	uint64_t			xd:1;
} PACKED PML4e;

typedef struct
{
	PML4e				entries[512];
} PACKED PML4;

typedef struct
{
	uint64_t			present:1;
	uint64_t			rw:1;
	uint64_t			user:1;
	uint64_t			pwt:1;
	uint64_t			pcd:1;
	uint64_t			accessed:1;
	uint64_t			zero:6;		// ignored and PS (page size = 0 for 4KB pages) and 4 ignored bits.
	uint64_t			pdPhysAddr:36;
	uint64_t			zero2:4;
	uint64_t			ignored:11;
	uint64_t			xd:1;
} PACKED PDPTe;

typedef struct
{
	PDPTe				entries[512];
} PACKED PDPT;

typedef struct
{
	uint64_t			present:1;
	uint64_t			rw:1;
	uint64_t			user:1;
	uint64_t			pwt:1;
	uint64_t			pcd:1;
	uint64_t			accessed:1;
	uint64_t			ignore:1;
	uint64_t			ps:1;
	uint64_t			moreIgnored:4;
	uint64_t			ptPhysAddr:36;
	uint64_t			zero:4;
	uint64_t			ignoredVeryVeryMuchLol:11;
	uint64_t			xd:1;
} PACKED PDe;

typedef struct
{
	PDe				entries[512];
} PACKED PD;

typedef struct
{
	uint64_t			present:1;
	uint64_t			rw:1;
	uint64_t			user:1;
	uint64_t			pwt:1;
	uint64_t			pcd:1;
	uint64_t			accessed:1;
	uint64_t			dirty:1;
	uint64_t			pat:1;
	uint64_t			global:1;
	uint64_t			ignored:3;
	uint64_t			framePhysAddr:36;
	uint64_t			zero:4;
	uint64_t			gx_r:1;				// PROT_READ
	uint64_t			gx_w:1;				// PROT_WRITE
	uint64_t			gx_x:1;				// PROT_EXEC
	uint64_t			gx_loaded:1;			// page was loaded ("present" may be 0 if PROT_NONE)
	uint64_t			gx_cow:1;			// copy this page upon write attempt
	uint64_t			gx_shared:1;			// the page is shared (else it is private)
	uint64_t			gx_perm_ovr:1;			// override default permissions (set by "mprotect")
	uint64_t			moreIgnored:4;
	uint64_t			xd:1;
} PACKED PTe;

typedef struct
{
	PTe				entries[512];
} PACKED PT;

PML4 *getPML4();
void refreshAddrSpace();

/**
 * Write data from the 'buffer' into the specified memory frame.
 */
void frameWrite(uint64_t frame, const void *buffer);

/**
 * Read data into the 'buffer' from the specified memory frame.
 */
void frameRead(uint64_t frame, void *buffer);

/**
 * Invalidate TLB entries for the specified address.
 */
void invlpg(void *addr);

#endif
