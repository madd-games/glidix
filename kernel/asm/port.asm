;	Glidix kernel
;
;	Copyright (c) 2014-2017, Madd Games.
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

[global outb]
outb:
	push rbp
	mov rbp, rsp
	mov dx,		di
	mov ax,		si
	out dx,		al
	pop rbp
	ret

[global inb]
inb:
	push rbp
	mov rbp, rsp
	mov dx,		di
	xor rax,	rax
	in  al,		dx
	pop rbp
	ret

[global outw]
outw:
	push rbp
	mov rbp, rsp
	mov dx,		di
	mov ax,		si
	out dx,		ax
	pop rbp
	ret

[global inw]
inw:
	push rbp
	mov rbp, rsp
	mov dx,		di
	xor rax,	rax
	in  ax,		dx
	pop rbp
	ret

[global outd]
outd:
	push rbp
	mov rbp, rsp
	mov dx,		di
	mov eax,	esi
	out dx,		eax
	pop rbp
	ret

[global ind]
ind:
	push rbp
	mov rbp, rsp
	mov dx,		di
	xor rax,	rax
	in  eax,	dx
	pop rbp
	ret

[global insw]
insw:
	push rbp
	mov rbp, rsp
	mov rcx,	rdx
	mov dx,		di
	mov rdi,	rsi
	rep insw
	pop rbp
	ret

[global outsw]
outsw:
	push rbp
	mov rbp, rsp
	mov rcx,	rdx
	mov dx,		di
	rep outsw
	pop rbp
	ret

[global outsb]
outsb:
	push rbp
	mov rbp, rsp
	mov rcx,	rdx
	mov dx,		di
	rep outsb
	pop rbp
	ret

[global insd]
insd:
	push rbp
	mov rbp, rsp
	mov rcx,	rdx
	mov dx,		di
	mov rdi,	rsi
	rep insd
	pop rbp
	ret
