;	Glidix dynamic linker
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

global _start
extern dynld_main
extern dynld_errno

section .text
_start:
	; errno location
	mov rdi, dynld_errno
	mov rax, 49
	syscall
	
	mov rdi, [rsp]
	lea rsi, [rsp+8]
	mov rdx, rdi
	inc rdx
	shl rdx, 3
	add rdx, rsi
	mov rcx, rsp		; pass the return stack as fourth argument
	and rsp, ~0xF		; align the stack
	call dynld_main
	
	mov rdi, rax
	mov rax, 0
	syscall

global write
write:
	mov rax, 1
	syscall
	ret

global open
open:
	mov rax, 4
	syscall
	ret

global pread
pread:
	mov r10, rcx
	mov rax, 52
	syscall
	ret

global mmap
mmap:
	mov r10, rcx
	mov rax, 54
	syscall
	ret

global munmap
munmap:
	mov r10, rcx
	mov rax, 99
	syscall
	ret

global close
close:
	mov rax, 5
	syscall
	ret

global _glidix_fstat
_glidix_fstat:
	mov rax, 37
	syscall
	ret

global raise
raise:
	mov rax, 18
	syscall
	ret

extern dynld_pltreloc
extern dynld_abort
global dynld_lazybind
dynld_lazybind:
	; entry point to the lazy binder
	xchg bx, bx
	xchg rdi, [rsp]			; library handle
	xchg rsi, [rsp+8]		; index
	
	; preserve the rest of the argument registers
	push rdx
	push rcx
	push r8
	push r9
	
	; perform the relocation
	call dynld_pltreloc
	
	; if it returned NULL, abort
	test rax, rax
	jz .fail
	
	; restore argument registers
	; because of the exchanges above, this will also return the stack to the state of the attempted
	; call, so we just have to jump to the function (rax) after that
	pop r9
	pop r8
	pop rcx
	pop rdx
	pop rdi
	pop rsi
	jmp rax

.fail:
	call dynld_abort

global dynld_enter
dynld_enter:
	mov rsp, rdi
	xor rbp, rbp
	xor rdx, rdx
	jmp rsi
