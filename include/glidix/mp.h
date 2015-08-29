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

#ifndef __glidix_mp_h
#define __glidix_mp_h

/**
 * MP Tables. Those are a simpler way of getting PCI routing info - perhaps until we fully implement ACPI?
 */

#include <glidix/common.h>

typedef struct
{
	char		sig[4];		/* "_MP_" */
	uint32_t	conftabPhys;
	uint8_t		length;		/* in 16-bytes */
	uint8_t		rev;
	uint8_t		checksum;
	uint8_t		features1;
	uint32_t	features;
} PACKED MPFloatPtr;

typedef struct
{
	char		sig[4];		/* "PCMP" */
	uint16_t	length;
	uint8_t		specrev;
	uint8_t		checksum;
	char		oemProduct[20];
	uint32_t	oemtabPhys;
	uint16_t	oemSize;
	uint16_t	entryCount;
	uint32_t	lapicPhys;
	uint16_t	extabLen;
	uint8_t		extabChecksum;
	uint8_t		rsv;
} PACKED MPConfigHeader;

typedef struct
{
	uint8_t		type;		/* 3 - I/O Interrupt Assignment Entry */
	uint8_t		inttype;
	uint16_t	flags;
	uint8_t		bus;
	uint8_t		irq;
	uint8_t		dstapic;
	uint8_t		intpin;
} PACKED MPIntAssignment;

typedef struct
{
	uint8_t		type;		/* 1 - Bus Entry */
	uint8_t		busid;
	char		bustype[6];
} PACKED MPBusEntry;

void mpInit();

#endif
