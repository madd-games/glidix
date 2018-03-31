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

#ifndef __glidix_fpu_h
#define __glidix_fpu_h

#include <stdint.h>

#define	MX_IE				(1 << 0)
#define	MX_DE				(1 << 1)
#define	MX_ZE				(1 << 2)
#define	MX_OE				(1 << 3)
#define	MX_UE				(1 << 4)
#define	MX_PE				(1 << 5)
#define	MX_DAZ				(1 << 6)
#define	MX_IM				(1 << 7)
#define	MX_DM				(1 << 8)
#define	MX_ZM				(1 << 9)
#define	MX_OM				(1 << 10)
#define	MX_UM				(1 << 11)
#define	MX_PM				(1 << 12)
#define	MX_RC_RN			0
#define	MX_RC_RNEG			0x2000
#define	MX_RC_RPOS			0x4000
#define	MX_RC_RZ			0x6000
#define	MX_RC_FZ			(1 << 15)

typedef struct
{
	//uint8_t block[512];
	uint16_t			fcw;
	uint16_t			fsw;
	uint8_t				ftw;
	/* 1-byte padding inserted by alignment */
	uint16_t			fop;
	uint32_t			fpuIP;
	uint16_t			cs;
	uint16_t			_rsv0;
	uint32_t			fpuDP;
	uint16_t			ds;
	uint16_t			_rsv1;
	uint32_t			mxcsr;
	uint32_t			mxcsrMask;
	uint8_t				block[512-32];
} ALIGN(16) FPURegs;

/* all functions implemented in fpu.asm */
void fpuInit();
void fpuSave(FPURegs *regs);
void fpuLoad(FPURegs *regs);
uint32_t fpuGetMXCSR();
void fpuSetMXCSR(uint32_t val);

#endif
