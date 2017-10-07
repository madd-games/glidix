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

#ifndef GXDBG_H
#define GXDBG_H

#define	TR_EXEC					0		/* exec called */
#define	TR_EXIT					1		/* thread terminated */
#define TR_SIGNAL				2		/* signal caught */

#define	TC_DEBUG				1		/* become a debugger */
#define	TC_GETREGS				2		/* get registers of 'thid' */
#define	TC_CONT					3		/* continue */
#define TC_SET_FLAGS				4		/* set debugging flags */

#define	DBG_STOP_ON_EXEC			(1 << 0)
#define	DBG_DEBUGGER				(1 << 1)	/* i am the debugger, set stop-on-exec in children */
#define	DBG_DEBUG_MODE				(1 << 2)	/* i am being debugged */
#define DBG_SIGNALS				(1 << 3)	/* trap when dispatching signals */

#define	SPACE_CHARS				" \t"

typedef struct
{
	char		data[512];
} FPURegs;

typedef struct
{
	FPURegs		fpuRegs;
	uint64_t	rflags;
	uint64_t	rip;
	uint64_t	rdi, rsi, rbp, rbx, rdx, rcx, rax;
	uint64_t	r8, r9, r10, r11, r12, r13, r14, r15;
	uint64_t	rsp;
	uint64_t	fsbase, gsbase;
} MachineState;

char* prompt();

#endif
