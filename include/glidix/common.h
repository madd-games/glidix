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

#ifndef __glidix_common_h
#define __glidix_common_h

#include <stdint.h>
#include <stddef.h>

/**
 * Some common routines used by various parts of the kernel.
 */

#define	ASM			__asm__ __volatile__
#define	panic(...)		_panic(__FILE__, __LINE__, __func__, __VA_ARGS__)
#define	PACKED			__attribute__ ((packed))
#define	BREAKPOINT()		ASM ("xchg %bx, %bx")
#define	ALIGN(x)		__attribute__ ((aligned(x)))
#define	PAGE_ALIGN		ALIGN(0x1000)
#define	ATOMIC(t)		ALIGN(sizeof(t)) t
#define	SECTION(n)		__attribute__ ((section(n)))
#define	PER_CPU			SECTION(".data_per_cpu")
#define	cli()			ASM ("cli")
#define	hlt()			ASM ("hlt")
#define	sti()			ASM ("sti")
#define	nop()			ASM ("nop")
#define	NT_SECS(secs)		((secs)*1000000000)
#define	NEW(type)		((type*)kmalloc(sizeof(type)))		/* only use after including <glidix/memory.h> */
#define	NEW_EX(type, size)	((type*)kmalloc(sizeof(type)+(size)))

void _panic(const char *filename, int lineno, const char *funcname, const char *fmt, ...);

typedef struct {
	uint64_t ds;
	uint64_t rdi, rsi, rbp, rbx, rdx, rcx, rax;
	uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t intNo;
	uint64_t errCode;
	uint64_t rip, cs, rflags, rsp, ss;
} PACKED Regs;

typedef struct
{
	uint32_t flags;
	uint32_t memLower;
	uint32_t memUpper;
	uint32_t bootDevice;
	uint32_t cmdLine;
	uint32_t modsCount;
	uint32_t modsAddr;
	uint8_t  ignore[16];
	uint32_t mmapLen;
	uint32_t mmapAddr;
} PACKED MultibootInfo;

typedef struct
{
	uint32_t		size;
	uint64_t		baseAddr;
	uint64_t		len;
	uint32_t		type;
} PACKED MultibootMemoryMap;

typedef struct
{
	uint32_t		modStart;
	uint32_t		modEnd;
} PACKED MultibootModule;

#ifndef _SYS_TYPES_H
typedef	uint64_t			dev_t;
typedef	uint64_t			ino_t;
typedef	uint64_t			mode_t;
typedef	uint64_t			nlink_t;
typedef	uint64_t			uid_t;
typedef	uint64_t			gid_t;
typedef	uint64_t			blksize_t;
typedef	uint64_t			blkcnt_t;
typedef	int64_t				clock_t;
typedef	int64_t				time_t;
typedef	int64_t				off_t;
typedef	int64_t				ssize_t;
typedef int				pid_t;
#endif

/* init.c */
typedef enum
{
	KERNEL_RUNNING,
	KERNEL_STOPPING,
} KernelStatus;
extern KernelStatus kernelStatus;

/* common.asm */
uint64_t	msrRead(uint32_t msr);
void		msrWrite(uint32_t msr, uint64_t value);

/**
 * Give up remaining CPU time (implemented in sched.c, don't ask for reasons).
 */
void		kyield();

/**
 * This is used to improve debugging of the OS inside Bochs. When this function is called,
 * and we are not running within Bochs, 0 is simply returned. If we ARE within Bochs, then
 * this issues a breakpoint such that the developer may change the value of RAX to give us
 * an option, without depending on any Glidix drivers, which can help debug some low-level
 * stuff. See common.asm.
 */
uint64_t getDebugInput();

/**
 * Enter the debugging state (see debug.c). The passed 'Regs' structure refers to the state
 * of the kernel at the time of a complete failure.
 */
void debugKernel(Regs *regs);

#endif
