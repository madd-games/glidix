;	Glidix kernel
;
;	Copyright (c) 2014, Madd Games.
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

section .text
bits 64

extern kfree
extern heapDump
global _jmp_usbs
_jmp_usbs:
	; move the stack to a userspace area.
	mov		rsp,		0x8000002000

	; free the temporary stack (passed to us as an argument, which we'll now, effectively,
	; pass to kfree).
	call		kfree

	; magic time!
	; we're going to jump to user mode, into the first byte of the loaded /initrd/usbs
	cli
	mov		ax,		0x23
	mov		ds,		ax
	mov		es,		ax
	mov		fs,		ax
	mov		gs,		ax

	push qword	0x23			; SS
	mov rax,	0x8000002000
	push rax				; RSP (we're loading it again)
	pushfq
	pop rax
	or rax, (1 << 9)			; enable interrupts
	push rax
	push qword	0x1B			; CS (usermode)
	mov  rax,	0x8000000000
	push rax				; RIP
	iretq
