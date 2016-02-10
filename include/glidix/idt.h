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

#ifndef __glidix_idt_h
#define __glidix_idt_h

#include <glidix/common.h>
#include <stdint.h>

// exceptions and APIC interrupt numbers
#define	I_DIV_ZERO			0
#define	I_DEBUG				1
#define	I_NMI				2
#define	I_BREAKPOINT			3
#define	I_OVERFLOW			4
#define	I_BOUND_EX			5
#define	I_UNDEF_OPCODE			6
#define	I_NODEV				7
#define	I_DOUBLE			8
#define	I_BAD_TSS			10
#define	I_NOSEG				11
#define	I_STACKSEGV			12
#define	I_GPF				13
#define	I_PAGE_FAULT			14
#define	I_FLOAT_EX			16
#define	I_ALIGN_CHECK			17
#define	I_MACHINE_CHECK			18
#define	I_SIMD_EX			19
#define	I_VIRT_EX			20
#define	I_SECURITY_EX			30

// IRQ interrupt numbers.
#define	IRQ0				32
#define	IRQ1				33
#define	IRQ2				34
#define	IRQ3				35
#define	IRQ4				36
#define	IRQ5				37
#define	IRQ6				38
#define	IRQ7				39
#define	IRQ8				40
#define	IRQ9				41
#define	IRQ10				42
#define	IRQ11				43
#define	IRQ12				44
#define	IRQ13				45
#define	IRQ14				46
#define	IRQ15				47

// PCI interrupts
#define	I_PCI0				48
#define	I_PCI1				49
#define	I_PCI2				50
#define	I_PCI3				51
#define	I_PCI4				52
#define	I_PCI5				53
#define	I_PCI6				54
#define	I_PCI7				55
#define	I_PCI8				56
#define	I_PCI9				57
#define	I_PCI10				58
#define	I_PCI11				59
#define	I_PCI12				60
#define	I_PCI13				61
#define	I_PCI14				62
#define	I_PCI15				63

// really weird stuff
#define	IRQ_IDE				64

#define	I_APIC_TIMER			65

typedef struct
{
	uint16_t limit;
	uint64_t addr;
} PACKED IDTPointer;

/**
 * I think the person at Intel designing this was high on something.
 */
typedef struct
{
	uint16_t offsetLow;
	uint16_t codeSegment;
	uint8_t  reservedIst;
	uint8_t  flags;
	uint16_t offsetMiddle;
	uint32_t offsetHigh;
	uint32_t reserved;
} PACKED IDTEntry;

/**
 * Takes the IRQ number (0..15) as the argument.
 */
typedef void (*IRQHandler)(int);

void initIDT();
void idtReboot();
IRQHandler registerIRQHandler(int irq, IRQHandler handler);

#endif
