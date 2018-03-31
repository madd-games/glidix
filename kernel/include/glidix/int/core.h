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

#ifndef __glidix_core_h
#define __glidix_core_h

/**
 * This file contains the file format, and functions to manipulate, Glidix core dump files.
 */

#include <glidix/util/common.h>
#include <glidix/thread/signal.h>

/**
 * Coredump header, located at the start of all core dumps.
 */
#define	CORE_MAGIC				(*((const uint32_t*)"CORE"))
typedef struct
{
	/**
	 * The magic number (CORE_MAGIC), i.e. the string "CORE"
	 */
	uint32_t				ch_magic;
	
	/**
	 * Size of the header in bytes. Indicates which fields are valid.
	 * (Restricted to 32 bits for alignment, and because the header should
	 * never grow larger than 4GB anyway).
	 */
	uint32_t				ch_size;
	
	/**
	 * Offset to, and number of entries in, the segment list, and the size of
	 * each entry.
	 */
	uint64_t				ch_segoff;
	uint64_t				ch_segnum;
	uint64_t				ch_segentsz;
	
	/**
	 * Offset to the "page tree". This is a structure similar to the PDPT, but it
	 * stores only offsets into the file of each lower table, instead of physical
	 * addresses and attributes. This structure only stores pages which are part
	 * of anonymous mappings (those where cs_offset is zero), and any pages not
	 * listed here, but part of a valid mapping, are assumed to be filled with zeroes.
	 */
	uint64_t				ch_pagetree;
	
	/**
	 * Offset to, and number of entries in, the thread list, and the size of
	 * each entry.
	 *
	 * The first entry is always the failing thread.
	 */
	uint64_t				ch_thoff;
	uint64_t				ch_thnum;
	uint64_t				ch_thentsz;
	
	/**
	 * General information about the process.
	 */
	int					ch_pid;			// process ID
	int					ch_sig;			// signal which caused the dump
	int					ch_code;		// si_code value for that signal
	int					ch_resv;		// reserved for now (aligns the following)
	uint64_t				ch_addr;		// si_addr value for that signal
	uid_t					ch_uid;			// real user ID
	gid_t					ch_gid;			// real group ID
	uid_t					ch_euid;		// effective user ID
	gid_t					ch_egid;		// effective group ID
} CoreHeader;

/**
 * Describes a segment in the segment list.
 */
typedef struct
{
	/**
	 * Virtual base address of the segment, page-aligned.
	 */
	uint64_t				cs_base;
	
	/**
	 * Size of the segment in bytes (always a multiple of the page size).
	 */
	uint64_t				cs_size;
	
	/**
	 * Permissions and flags of the segments ('prot' and 'flags' from mmap()).
	 */
	int					cs_prot;
	int					cs_flags;
	
	/**
	 * Offset to where in this file the contents of this segment are stored. If zero,
	 * then the contents must be looked up using the "page tree" instead (and all
	 * non-present pages within the range are assumed to be filled with zeroes).
	 */
	uint64_t				cs_offset;
} CoreSegment;

/**
 * Page table (a level of the page tree).
 */
typedef struct
{
	uint64_t				cpt_entries[512];
} CorePageTable;

/**
 * Describes a thread state in the thread list.
 */
typedef struct
{
	/**
	 * FPU registers (as given by FXSAVE).
	 */
	uint8_t					ct_fpu[512];
	
	/**
	 * Thread ID.
	 */
	int					ct_thid;
	
	/**
	 * Nice value.
	 */
	int					ct_nice;
	
	/**
	 * Values of registers (this is in the CPU's register numbering order).
	 */
	uint64_t				ct_rax;
	uint64_t				ct_rcx;
	uint64_t				ct_rdx;
	uint64_t				ct_rbx;
	uint64_t				ct_rsp;
	uint64_t				ct_rbp;
	uint64_t				ct_rsi;
	uint64_t				ct_rdi;
	uint64_t				ct_r8;
	uint64_t				ct_r9;
	uint64_t				ct_r10;
	uint64_t				ct_r11;
	uint64_t				ct_r12;
	uint64_t				ct_r13;
	uint64_t				ct_r14;
	uint64_t				ct_r15;
	
	/**
	 * Hidden registers now.
	 */
	uint64_t				ct_rip;
	uint64_t				ct_rflags;
	uint64_t				ct_fsbase;
	uint64_t				ct_gsbase;
	
	/**
	 * The virtual address at which this thread stores 'errno'.
	 */
	uint64_t				ct_errnoptr;
} CoreThread;

/**
 * Do a coredump as a result of the specified signal with the specified userspace registers.
 */
void coredump(siginfo_t *si, Regs *regs);

#endif
