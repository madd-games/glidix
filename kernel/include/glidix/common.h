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
#define	FORMAT(a, b, c)		__attribute__ ((format(a, b, c)))
#define	cli()			ASM ("cli")
#define	hlt()			ASM ("hlt")
#define	sti()			ASM ("sti")
#define	nop()			ASM ("nop")
#define	NT_SECS(secs)		((secs)*1000000000UL)
#define	NT_MILLI(milli)		((milli)*1000000UL)
#define	NEW(type)		((type*)kmalloc(sizeof(type)))		/* only use after including <glidix/memory.h> */
#define	NEW_EX(type, size)	((type*)kmalloc(sizeof(type)+(size)))
#define	kalloca(x)		__builtin_alloca(x)
#define	htons			__builtin_bswap16
#define	htonl			__builtin_bswap32
#define	ntohs			__builtin_bswap16
#define	ntohl			__builtin_bswap32

/**
 * Special traps. If a userspace process jumps to one of these, it will trigger
 * a page fault with a faultAddr equal to the given trap, and with the "fetch" flag
 * set. The kernel will then perform a specific job, as if a function was called.
 */
#define	TRAP_SIGRET			0xFFFFFFFFFFFF0000

void _panic(const char *filename, int lineno, const char *funcname, const char *fmt, ...);

typedef struct
{
	uint64_t ds;			// 0x00
	uint64_t rdi;			// 0x08
	uint64_t rsi;			// 0x10
	uint64_t rbp;			// 0x18
	uint64_t rbx;			// 0x20
	uint64_t rdx;			// 0x28
	uint64_t rcx;			// 0x30
	uint64_t rax;			// 0x38
	uint64_t r8;			// 0x40
	uint64_t r9;			// 0x48
	uint64_t r10;			// 0x50
	uint64_t r11;			// 0x58
	uint64_t r12;			// 0x60
	uint64_t r13;			// 0x68
	uint64_t r14;			// 0x70
	uint64_t r15;			// 0x78
	uint64_t intNo;			// 0x80
	uint64_t errCode;		// 0x88
	uint64_t rip;			// 0x90
	uint64_t cs;			// 0x98
	uint64_t rflags;		// 0xA0
	uint64_t rsp;			// 0xA8
	uint64_t ss;			// 0xB0
} PACKED Regs;

typedef struct
{
	uint32_t		size;
	uint64_t		baseAddr;
	uint64_t		len;
	uint32_t		type;
} PACKED MultibootMemoryMap;

typedef struct
{
	uint64_t			features;
	uint64_t			kernelMain;
	uint64_t			gdtPointerVirt;
	uint32_t			pml4Phys;
	uint32_t			mmapSize;
	MultibootMemoryMap*		mmap;
	uint64_t			initrdSize;
	uint64_t			end;
	uint64_t			initrdSymtabOffset;
	uint64_t			initrdStrtabOffset;
	uint64_t			numSymbols;
} KernelBootInfo;

typedef struct
{
	uint32_t ignore;
	uint64_t rsp0;
} PACKED TSS;

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

extern KernelBootInfo *bootInfo;

uint64_t	getFlagsRegister();
void		setFlagsRegister(uint64_t flags);

/* common.asm */
uint64_t	msrRead(uint32_t msr);
void		msrWrite(uint32_t msr, uint64_t value);

/**
 * Print a stack trace from the specified stack frame.
 */
void stackTrace(uint64_t rip, uint64_t rbp);

/**
 * Print the stack trace of the call to this function.
 */
void stackTraceHere();

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

/**
 * Atomically store a value at a pointer, and return the old value; defined in common.asm.
 */
uint8_t atomic_swap8(void *ptr, uint8_t newval);
uint16_t atomic_swap16(void *ptr, uint16_t newval);
uint32_t atomic_swap32(void *ptr, uint32_t newval);
uint64_t atomic_swap64(void *ptr, uint64_t newval);

/**
 * Atomic AND.
 */
void atomic_and8(uint8_t *ptr, uint8_t and_with);

/**
 * Atomic test-and-set.
 */
int atomic_test_and_set8(uint8_t *ptr, int bitpos);
int atomic_test_and_set(void *ptr, int bitpos);

/**
 * Atomically test whether the value at ptr is equal to 'oldval', and if so, set it to 'newval'.
 * Returns the original value at 'ptr'.
 */
uint8_t atomic_compare_and_swap8(void *ptr, uint8_t oldval, uint8_t newval);
uint16_t atomic_compare_and_swap16(void *ptr, uint16_t oldval, uint16_t newval);
uint32_t atomic_compare_and_swap32(void *ptr, uint32_t oldval, uint32_t newval);
uint64_t atomic_compare_and_swap64(void *ptr, uint64_t oldval, uint64_t newval);

/**
 * Those are implemented in acpi/acglidix.c because reusing code is cool and who cares about
 * the mess amirite?
 */
void* mapPhysMemory(uint64_t phaddr, uint64_t len);
void unmapPhysMemory(void *laddr, uint64_t len);

/**
 * Defined in sched.c. Initializes a register structure for userspace.
 */
void initUserRegs(Regs *regs);

/**
 * Defined in sched.c. Changes a register structure to make it kernel-space.
 */
void switchToKernelSpace(Regs *regs);

/**
 * Defined in common.asm. Call this to let the CPU cool off. This preserves RFLAGS (along with the IF),
 * then enables interrupts and halts. Returns with the old value of the interrupt flag.
 */
void cooloff();

#endif
