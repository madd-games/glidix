/*
	Glidix kernel

	Copyright (c) 2014, Madd Games.
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

#ifndef __glidix_common_h
#define __glidix_common_h

#include <stdint.h>

/**
 * Some common routines used by various parts of the kernel.
 */

#define	ASM			__asm__ __volatile__
#define	panic(...)		_panic(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define	PACKED			__attribute__ ((packed))
#define	BREAKPOINT()		ASM ("xchg %bx, %bx")

void _panic(const char *filename, int lineno, const char *funcname, const char *fmt, ...);

typedef struct {
	uint64_t ds;
	uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
	uint64_t  intNo;
	uint64_t errCode;
	uint64_t rip, cs, rflags, rsp, ss;
} PACKED Regs;

typedef struct
{
	uint32_t flags;
	uint32_t memLower;
	uint32_t memUpper;
} PACKED MultibootInfo;

#endif
