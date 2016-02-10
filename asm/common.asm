;	Glidix kernel
;
;	Copyright (c) 2014-2016, Madd Games.
;	All rights reserved.
;	
;	Redistribution and use in source and binary forms, with or without
;	modification, are permitted provided that the following conditions are met:
;	
;	* Redistributions of source code must retain the above copyright notice, this
;	  list of conditions and the following disclaimer.
;	
;	* Redistributions in binary form must reproduce the above copyright notice,
;	  this list of conditions and the following disclaimer in the documentation
;	  and/or other materials provided with the distribution.
;	
;	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
;	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
;	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
;	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
;	FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
;	DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
;	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
;	CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
;	OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
;	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

bits 64

section .text

[global msrWrite]
msrWrite:
	mov	ecx,		edi
	mov	rdx,		rsi
	shr	rdx,		32
	mov	eax,		esi
	wrmsr
	ret

[global msrRead]
msrRead:
	mov	ecx,		edi
	rdmsr
	rol	rax,		32
	mov	eax,		edx
	rol	rax,		32
	ret

[global getDebugInput]
getDebugInput:
	xor	rax,		rax
	xchg	bx,		bx
	ret

[extern debugKernel]
[extern kernelDead]
[global trapKernel]
trapKernel:
	cli
	mov	r14,		rsp
	mov	rsp,		rescueStack + 0x4000

	; make a register structure
	mov	r13,		rax
	mov	ax,		ss
	push	rax
	mov	rax,		r14
	push	rax
	pushf
	mov	ax,		cs
	push	rax
	push	r15				; R15 should be set to 'return RIP'
	xor	rax,		rax
	push	rax
	push	rax
	push	r15
	push	r14
	push	r13
	push	r12
	push	r11
	push	r10
	push	r9
	push	r8
	mov	rax,		r13
	push	rax
	push	rcx
	push	rdx
	push	rbx
	push	rbp
	push	rsi
	push	rdi
	mov	ax,		ds
	push	rax

	; mark the kernel dead
	mov	rdi,		qword kernelDead
	mov	[rdi],		dword 1

	; go to the debugger
	mov	rdi,		rsp
	and	rsp,		~0xF
	call	debugKernel

[global atomic_swap8]
atomic_swap8:
	mov		cx,	si
	lock xchg	[rdi],	cl
	mov		rax,	rcx
	ret

[global atomic_swap16]
atomic_swap16:
	lock xchg	[rdi],	si
	mov		ax,	si
	ret

[global atomic_swap32]
atomic_swap32:
	lock xchg	[rdi],	esi
	mov		eax,	esi
	ret

[global atomic_swap64]
atomic_swap64:
	lock xchg	[rdi],	rsi
	mov		rax,	rsi
	ret

section .bss
align 16
rescueStack resb 0x4000
