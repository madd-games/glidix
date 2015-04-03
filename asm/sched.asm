;	Glidix kernel
;
;	Copyright (c) 2014-2015, Madd Games.
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

section .text
bits 64

[global getFlagsRegister]
getFlagsRegister:
	pushfq
	pop rax
	ret

[global switchContext]
switchContext:
	; The argument is stored in RDI, and is the address of a Regs structure.
	; If we move the stack there, we can easily do a context switch with a
	; bunch of pops.
	cli
	mov	rsp,		rdi

	; first we switch the DS
	pop	rbx
	mov	ds,		bx
	mov	es,		bx
	mov	fs,		bx
	mov	gs,		bx

	; GPRs
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

	; ignore "intNo" and "errCode"
	add	rsp,		16

	; the rest is popped by an interrupt return
	iretq

[global enterDebugContext]
enterDebugContext:
	; essentially the same as switchContext, but we trap into the Bochs debugger
	; so that we can see why stuff is breaking in the context.
	cli
	mov	rsp,		rdi

	; first we switch the DS
	pop	rbx
	mov	ds,		bx
	mov	es,		bx
	mov	fs,		bx
	mov	gs,		bx

	; GPRs
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

	; ignore "intNo" and "errCode"
	add	rsp,		16

	; the rest is popped by an interrupt return, but obviously trap first.
	xchg	bx,		bx
	iretq

[global reloadTR]
[extern _tss_reload_access]
reloadTR:
	call	_tss_reload_access
	mov	rax,		0x2b
	ltr	ax
	ret
