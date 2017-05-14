;	Glidix kernel
;
;	Copyright (c) 2014-2017, Madd Games.
;	All rights reserved.
;	
;	Redistribution and use in source and binary forms, with or without
;	modification, are permitted provided that the following conditions are met:
;	
;	* Redistributions of source code must retain the above copyright notice, this
;		list of conditions and the following disclaimer.
;	
;	* Redistributions in binary form must reproduce the above copyright notice,
;		this list of conditions and the following disclaimer in the documentation
;		and/or other materials provided with the distribution.
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

; macros for manipulating the Regs struct.
%define	MSR_FS_BASE		0xC0000100
%define	MSR_GS_BASE		0xC0000101

; push the value of an MSR. Trashes RCX, RDX and RAX.
%macro pushMSR 1
	mov	rcx,	%1
	rdmsr
	shl	rdx,	32
	or	rdx,	rax
	test	rdx,	rdx
	jnz __bad_one
	push	rdx
%endmacro

; pop the value of an MSR.
%macro popMSR 1
	pop	rax
	test	rax,	rax
	jnz __bad_one_pop
	mov	rdx,	rax
	shr	rdx,	32
	mov	rcx,	%1
	wrmsr
%endmacro

; push all regs (except the ones pushed by IRET, and intNo/errCode)
%macro pushAll 0
	push	r15
	push	r14
	push	r13
	push	r12
	push	r11
	push	r10
	push	r9
	push	r8
	push	rax
	push	rcx
	push	rdx
	push	rbx
	push	rbp
	push	rsi
	push	rdi
	mov	ax,	ds
	push	rax
	pushMSR	MSR_GS_BASE
	pushMSR	MSR_FS_BASE
%endmacro

; same but reverse
%macro popAll 0
	popMSR	MSR_FS_BASE
	popMSR	MSR_GS_BASE
	pop	rax
	mov	ds,	ax
	mov	es,	ax
	pop	rdi
	pop	rsi
	pop	rbp
	pop	rbx
	pop	rdx
	pop	rcx
	pop	rax
	pop	r8
	pop	r9
	pop	r10
	pop	r11
	pop	r12
	pop	r13
	pop	r14
	pop	r15
%endmacro

[extern _panic]
__bad_one:
	mov rdi, __le_format
	mov rsi, 0
	mov r8, rdx
	mov rdx, __le_format
	mov rcx, __le_format
	call _panic

__bad_one_pop:
	mov rdi, __le_format2
	mov rsi, 0
	mov r8, rax
	mov rdx, __le_format2
	mov rcx, __le_format2
	call _panic
	
__le_format db '0x%016lX', 0
__le_format2 db 'POP 0x%016lX', 0
