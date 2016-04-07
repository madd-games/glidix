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

section .text
bits 64

extern currentThread
extern sysNumber
extern sysTable
extern sysEpilog
extern _kfree
extern heapDump
extern sysInvalid

global _jmp_usbs
_jmp_usbs:
	; move the stack to a userspace area.
	mov		rsp,		0x3000

	; free the temporary stack (passed to us as an argument, which we'll now, effectively,
	; pass to kfree).
	mov		rsi,		syscall_name
	mov		rdx,		41
	call		_kfree

	cli
	; magic time!
	; we're going to jump to user mode, into the first byte of the loaded /initrd/usbs
	mov		ax,		0x23
	mov		ds,		ax
	mov		es,		ax
	mov		fs,		ax
	mov		gs,		ax

	push qword	0x23			; SS
	mov rax,	0x3000
	push rax				; RSP (we're loading it again)
	pushfq
	pop rax
	or rax, (1 << 9)			; enable interrupts
	push rax
	push qword	0x1B			; CS (usermode)
	mov  rax,	0x1000
	push rax				; RIP
	iretq

global _syscall_entry
_syscall_entry:
	; the fourth argument, that is normally in RCX, is passed in R10 in this context
	; as RCX contains the return address
	xchg rax, r11
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	xor ax, ax
	mov ss, ax
	xchg rax, r11
	
	mov r11, currentThread
	mov r11, [r11]				; get the pointer
	mov [r11+0x208], rbx
	mov [r11+0x210], rsp
	mov [r11+0x218], rbp
	mov [r11+0x220], r12
	mov [r11+0x228], r13
	mov [r11+0x230], r14
	mov [r11+0x238], r15
	mov [r11+0x240], rcx			; return RIP
	mov [r11+0x248], dword 0		; errno
	mov r11, [r11+512]			; get syscall stack pointer
	
	; preserve userspace stack pointer while loading kernel stack pointer
	xchg r11, rsp
	
	; it's now safe to enable interrupts
	sti
	
	; preserve user stack pointer and return address on the kernel stack
	push rcx			; return RIP
	push r11			; user RSP

	; move the fourth argument back into RCX
	mov rcx, r10
	
	; make sure the system call number is within bounds
	mov r10, sysNumber
	mov r10, [r10]
	cmp rax, r10
	jae .invalid
	
	mov r10, sysTable
	shl rax, 3			; multiply RAX by 8
	add rax, r10
	mov rax, [rax]
	
	test rax, rax
	jz .invalid			; entry is NULL
	
	; invoke the system call
	call rax
	
	; we must always perform the epilog
	mov rdi, rax
	call sysEpilog
	
	; restore user stack pointer into R10, return RIP into RCX, and we can do a sysret.
	pop r10
	pop rcx
	
	; disable interrupts, and store the return RFLAGS in R11, where we set IF=1
	pushf
	pop r11
	
	cli
	mov rsp, r10
	mov dx, 0x23
	mov ds, dx
	mov es, dx
	mov fs, dx
	mov gs, dx
	o64 sysret
	
	.invalid:
	call sysInvalid

section .data
syscall_name db 'syscall.asm', 0
