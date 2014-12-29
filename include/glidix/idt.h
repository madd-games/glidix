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

#ifndef __glidix_idt_h
#define __glidix_idt_h

#include <glidix/common.h>
#include <stdint.h>

#define	IRQ0	32
#define	IRQ1	33
#define	IRQ2	34
#define	IRQ3	35
#define	IRQ4	36
#define	IRQ5	37
#define	IRQ6	38
#define	IRQ7	39
#define	IRQ8	40
#define	IRQ9	41
#define	IRQ10	42
#define	IRQ11	43
#define	IRQ12	44
#define	IRQ13	45
#define	IRQ14	46
#define	IRQ15	47

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
IRQHandler registerIRQHandler(int irq, IRQHandler handler);

#endif
