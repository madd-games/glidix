/*
	Glidix Debugger

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

#ifndef CORE_H_
#define CORE_H_

/**
 * See the kernel (glidix/core.h) for information on the file format.
 */
#define	CORE_MAGIC				(*((const uint32_t*)"CORE"))
typedef struct
{
	uint32_t				ch_magic;
	uint32_t				ch_size;
	uint64_t				ch_segoff;
	uint64_t				ch_segnum;
	uint64_t				ch_segentsz;
	uint64_t				ch_pagetree;
	uint64_t				ch_thoff;
	uint64_t				ch_thnum;
	uint64_t				ch_thentsz;
	int					ch_pid;
	int					ch_sig;
	int					ch_code;
	int					ch_resv;
	uint64_t				ch_addr;
	uid_t					ch_uid;
	gid_t					ch_gid;
	uid_t					ch_euid;
	gid_t					ch_egid;
} CoreHeader;

typedef struct
{
	uint64_t				cs_base;
	uint64_t				cs_size;
	int					cs_prot;
	int					cs_flags;
	uint64_t				cs_offset;
} CoreSegment;

typedef struct
{
	uint64_t				cpt_entries[512];
} CorePageTable;

typedef struct
{
	uint8_t					ct_fpu[512];
	int					ct_thid;
	int					ct_nice;
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
	uint64_t				ct_rip;
	uint64_t				ct_rflags;
	uint64_t				ct_fsbase;
	uint64_t				ct_gsbase;
	uint64_t				ct_errnoptr;
} CoreThread;

/**
 * Go to core inspection mode. Returns the final exit status.
 */
int inspectCore(const char *filename);

#endif
