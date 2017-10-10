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

extern sysNumber
extern sysTable
extern sysEpilog
extern sysInvalid

global _syscall_entry
_syscall_entry:
	; the fourth argument, that is normally in RCX, is passed in R10 in this context
	; as RCX contains the return address
	swapgs					; GS.base = currentThread
	mov [gs:0x208], rbx
	mov [gs:0x210], rsp
	mov [gs:0x218], rbp
	mov [gs:0x220], r12
	mov [gs:0x228], r13
	mov [gs:0x230], r14
	mov [gs:0x238], r15
	mov [gs:0x240], rcx			; return RIP
	mov [gs:0x248], dword 0			; errno
	mov rsp, [gs:512]			; get syscall stack pointer
	push qword [gs:0x210]			; push user stack pointer
	swapgs
	
	; preserve other stuff
	push rcx				; return RIP
	push rbp				; RBP (to create a stack frame)
	mov rbp, rsp
	push r11				; return RFLAGS
	push r9					; R9 needs to be preserved for resettable system calls
	push r9					; push again to align stack
	
	; segments and enabling interrupts
	mov cx, 0x10
	mov ds, cx
	mov es, cx
	xor cx, cx
	mov ss, cx
	sti
	
	; fourth argument back into RCX
	mov rcx, r10
	
	; make sure the system call number is within bounds
	mov r10, sysNumber
	mov r10, [r10]
	cmp rax, r10
	jae .invalid
	
	; find the system call pointer (it is never NULL; unused slots map to
	; sysInvalid now!)
	mov r10, sysTable
	call [r10+8*rax]

	; we must always perform the epilog
	mov rdi, rax
	call sysEpilog

	; restore
	pop r9					; was pushed for stack alignment
	pop r9
	pop r11
	pop rbp
	pop rcx
	cli
	pop rsp
	mov dx, 0x23
	mov ds, dx
	mov es, dx
	o64 sysret
	
.invalid:
	call sysInvalid

section .data
syscall_name db 'syscall.asm', 0
