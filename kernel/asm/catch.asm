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

extern currentThread
extern _panic
global catch
global throw
global uncatch

section .rodata
str_panic db 'uncaught exception', 0
str_filename db 'catch.asm', 0
str_funcname db 'throw'

section .text
catch:
	; set R10 to the stack pointer before the call
	mov	r10,			rsp
	add	r10,			8
	
	; set R11 to the return RIP
	mov	r11,			[rsp]
	
	mov	rax,			currentThread
	mov	rax,			[rax]
	add	rax,			0x250
	mov	[rax+0],		rbx
	mov	[rax+8],		r10			; RSP before call
	mov	[rax+16],		rbp
	mov	[rax+24],		r12
	mov	[rax+32],		r13
	mov	[rax+40],		r14
	mov	[rax+48],		r15
	mov	[rax+56],		r11			; return RIP
	
	xor	rax,			rax
	ret
	
throw:
	; see if we're even catching exceptions
	mov	rax,			currentThread
	mov	rax,			[rax]
	add	rax,			0x250
	mov	rcx,			[rax+56]
	test	rcx,			rcx
	jz	throw_unhandled
	
	; restore all registers
	mov	rbx,			[rax+0]
	mov	rsp,			[rax+8]
	mov	rbp,			[rax+16]
	mov	r12,			[rax+24]
	mov	r13,			[rax+32]
	mov	r14,			[rax+40]
	mov	r15,			[rax+48]
	mov	r10,			[rax+56]		; return RIP in R10
	xor	rcx,			rcx
	mov	[rax+56],		rcx			; uncatch
	
	; now the stack is back at the catch() caller, so put the exception in RAX,
	; and jump back to the return RIP.
	mov	rax,			rdi
	jmp	r10

throw_unhandled:
	ret

uncatch:
	mov	rax,			currentThread
	mov	rax,			[rax]
	add	rax,			0x250
	xor	rcx,			rcx
	mov	[rax+56],		rcx
	ret
