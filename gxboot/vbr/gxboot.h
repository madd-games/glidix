/*
	Glidix bootloader (gxboot)

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

#ifndef GXBOOT_H
#define GXBOOT_H

typedef unsigned char			byte_t;
typedef	unsigned short			word_t;
typedef	unsigned int			dword_t;
typedef	unsigned long long		qword_t;

typedef struct
{
	byte_t				size;
	byte_t				unused;
	word_t				numSectors;
	word_t				offset;
	word_t				segment;
	qword_t				lba;
} DAP;

typedef struct
{
	byte_t				sbMagic[8];
	byte_t				sbMGSID[16];
	qword_t				sbFormatTime;
	qword_t				sbTotalBlocks;
	qword_t				sbChecksum;
	qword_t				sbUsedBlocks;
	qword_t				sbFreeHead;
	byte_t				sbPad[512-0x40];
} Superblock;

extern DAP dap;
extern byte_t sectorBuffer[];

void memset(void *buffer, unsigned char b, unsigned int size);
void memcpy(void *dest, const void *src, unsigned int size);
int  memcmp(const void *a, const void *b, unsigned int size);

/**
 * Call the BIOS to read a sector from disk. Implemented in entry.asm.
 * Arguments taken from DAP, returned in sectorBuffer. Calls INT 0x18 if
 * read failed.
 */
void biosRead();

#endif
