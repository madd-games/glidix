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

#ifndef __glidix_acpi_h
#define __glidix_acpi_h

#include <glidix/common.h>

typedef struct
{
	char					sig[8];
	uint8_t					checksum;
	char					oemid[6];
	uint8_t					rev;
	uint32_t				rsdtAddr;

	// ACPI 2.0
	uint32_t				len;
	uint64_t				xsdtAddr;
	uint8_t					extChecksum;
	uint8_t					rsv[3];
} PACKED ACPI_RSDPDescriptor;

typedef struct
{
	char					sig[4];
	uint32_t				len;
	uint8_t					rev;
	uint8_t					checksum;
	char					oemid[6];
	char					oemtabid[8];
	uint32_t				oemrev;
	uint32_t				crid;
	uint32_t				crev;
} PACKED ACPI_SDTHeader;

typedef struct
{
	uint8_t					type;
	uint8_t					len;
} PACKED MADTRecordHeader;

typedef struct
{
	uint8_t					apicID;
	uint8_t					rsv;
	uint32_t				ioapicbase;
	uint32_t				intbase;
} PACKED MADT_IOAPIC;

void acpiInit();

#endif
