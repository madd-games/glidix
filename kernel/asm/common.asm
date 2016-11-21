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
[extern switchTask]
[extern stackTrace]

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
	mov	rdi,		0xFFFFFFFF00000000
	and	rax,		rdi
	mov	rdi,		0x00000000FFFFFFFF
	and	rdx,		rdi
	or	rax,		rdx
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

[global atomic_test_and_set]
atomic_test_and_set:
	mov		rcx,	rsi
	xor		rax,	rax
	lock bts	[rdi],	rcx
	setc		al
	ret

; atomic_compare_and_swap(ptr, oldval, newval)
;	old = *ptr;
;	if ((*ptr) == oldval) *ptr = newval;
;	return old;
[global atomic_compare_and_swap64]
atomic_compare_and_swap64:
	mov		rax,	rsi
	lock cmpxchg	[rdi],	rdx
	ret

[global atomic_compare_and_swap32]
atomic_compare_and_swap32:
	mov		rax,	rsi
	lock cmpxchg	[rdi],	edx
	ret

[global atomic_compare_and_swap16]
atomic_compare_and_swap16:
	mov		rax,	rsi
	lock cmpxchg	[rdi],	dx
	ret

[global atomic_compare_and_swap8]
atomic_compare_and_swap8:
	mov		rax,	rsi
	lock cmpxchg	[rdi],	dl
	ret

[global atomic_and8]
atomic_and8:
	mov		rax,	rsi
	lock and	[rdi],	al
	ret

[global atomic_test_and_set8]
atomic_test_and_set8:
	xor		rax,	rax
	lock bts	[rdi],	rsi
	setc		al
	ret

[global _preempt]
_preempt:
	; allocate a Regs structure on the stack.
	sub rsp, 0xC0
	mov rdi, rsp
	
	; segment registers; DS=0x10, CS=0x08, SS=0
	mov rax, 0x10
	mov [rdi+0x00], rax
	xor rax, rax
	mov [rdi+0xB0], rax
	mov rax, 0x08
	mov [rdi+0x98], rax
	
	; callee-save registers
	mov [rdi+0x20], rbx
	mov [rdi+0x18], rbp
	mov [rdi+0x60], r12
	mov [rdi+0x68], r13
	mov [rdi+0x70], r14
	mov [rdi+0x78], r15
	
	; store the return RSP in the appropriate field. the return RSP is
	; 0xC0+8 bytes below the current stack, as we've got the Regs and
	; return RIP pushed
	mov rax, rsp
	add rax, 0xC8
	mov [rdi+0xA8], rax
	mov rax, [rsp+0xC0]
	mov [rdi+0x90], rax		; return RIP
	pushf
	pop rax
	mov [rdi+0xA0], rax
	
	; to enable proper unwinding of this, push the return RIP onto the stack,
	; followed by RBP and then set RBP to RSP
	push qword [rdi+0x90]
	push rbp
	mov rbp, rsp
	
	; switch task (the Regs structure is already in RDI).
	call switchTask
	
	; stack popped by the context switch
	ret

[global cooloff]
cooloff:
	pushf
	sti
	hlt
	popf
	ret

[global stackTraceHere]
stackTraceHere:
	mov rdi, [rsp]
	mov rsi, rbp
	call stackTrace
	ret

section .bss
align 16
rescueStack resb 0x4000
