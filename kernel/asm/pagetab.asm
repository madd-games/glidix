;	Glidix kernel
;
;	Copyright (c) 2014-2016, Madd Games.
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

[global refreshAddrSpace]
refreshAddrSpace:
	mov	rax,	cr3
	mov	cr3,	rax
	ret
	
[global ispZero]
ispZero:
	xor	rax,	rax
	mov	rdi,	0xFFFF808000000000
	mov	rcx,	512			; 512 qwords per page (4KB)
	rep stosq
	ret

[global frameWrite]
frameWrite:
	push	rbp
	mov	rbp, rsp
	
	; turn the frame address that was passed in into a valid PTE
	shl	rdi, 12
	or	rdi, 3	; present, write
	
	; allocate a page-aligned page-sized region on the stack to temporarily remap
	sub	rsp, 0x1000
	and	rsp, ~0xFFF
	
	; find the virtual address of the PTE (thanks to recursive mapping); put that in RDX
	mov	rdx, rsp
	mov	rax, 0xffffff8000000000
	shr	rdx, 9
	or	rdx, rax
	
	; preserve old PTE in R8, write the new PTE in
	mov	r8, [rdx]
	mov	[rdx], rdi
	
	; invalidate that page
	invlpg	[rsp]
	
	; do the copying (buffer already in RSI)
	mov	rdi, rsp
	mov	rcx, 0x200	; 512 qwords in a page
	rep movsq
	
	; restore the old PTE and invalidate the page again
	mov	[rdx], r8
	invlpg	[rsp]
	
	; thanks
	mov	rsp, rbp
	pop	rbp
	ret

[global frameRead]
frameRead:
	push	rbp
	mov	rbp, rsp
	
	; turn the frame address that was passed in into a valid PTE
	shl	rdi, 12
	or	rdi, 3	; present, write
	
	; allocate a page-aligned page-sized region on the stack to temporarily remap
	sub	rsp, 0x1000
	and	rsp, ~0xFFF
	
	; find the virtual address of the PTE (thanks to recursive mapping); put that in RDX
	mov	rdx, rsp
	mov	rax, 0xffffff8000000000
	shr	rdx, 9
	or	rdx, rax
	
	; preserve old PTE in R8, write the new PTE in
	mov	r8, [rdx]
	mov	[rdx], rdi
	
	; invalidate that page
	invlpg	[rsp]
	
	; do the copying
	mov	rdi, rsi
	mov	rsi, rsp
	mov	rcx, 0x200
	rep movsq
	
	; restore the old PTE and invalidate the page again
	mov	[rdx], r8
	invlpg	[rsp]
	
	; thanks
	mov	rsp, rbp
	pop	rbp
	ret

[global invlpg]
invlpg:
	invlpg	[rdi]
	ret

