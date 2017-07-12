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

extern apEntry
global trampoline_start
global trampoline_end

section .text

trampoline_start:
bits 16
jmp 0:0xA005
cli
xor ax, ax
mov ds, ax
mov es, ax
mov ss, ax

; release the flagAP2BSP to tell the BSP that we've booted
mov [0xB025], al

; wait for the BSP to tell us that we may continue booting
.bspwait:
mov al, 1
xchg [0xB026], al
test al, 1
jnz .bspwait		; relative, so it's OK

; enable paging and PAE
mov eax, 10100000b
mov cr4, eax

; set the PML4 to the temporary one, because we can't yet load a 64-bit PML4 address.
mov eax, 0xC000
mov cr3, eax

; enable long mode in the EFER MSR
mov ecx, 0xC0000080
rdmsr
or eax, 0x00000100
wrmsr

; enable paging and protected mode at once
mov eax, cr0
or eax, 0x80000001
mov cr0, eax

; load the "low memory GDT"
lgdt [0xB000]

; jump to 64-bit mode
jmp 0x08:(trampoline64 - trampoline_start + 0xA000)

bits 64
trampoline64:
	; load 64-bit segments
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	xor ax, ax
	mov ss, ax
	
	; modify the GDT pointer to point to higher memory
	mov rax, [0xB002]
	mov rbx, 0xFFFF800000000000
	add rax, rbx
	mov [0xB002], rax
	lgdt [0xB000]
	
	; use the real PML4
	mov rax, [0xB014]
	mov cr3, rax
	
	; flush
	mov rax, cr0
	mov cr0, rax
	
	; load the IDT
	lidt [0xB00A]
	
	; stack
	xor rbp, rbp
	mov rsp, [0xB01C]
	
	; tell the BSP that we've collected all this information
	; by releasing the exit lock
	xor al, al
	mov [0xB024], al
	
	mov rax, apEntry
	call rax
	
trampoline_end:

[global loadLocalGDT]
[extern localGDT]
loadLocalGDT:
	sub rsp, 10
	mov [rsp], word 63	; the limit
	mov rax, qword localGDT
	mov [rsp+2], rax	; base
	lgdt [rsp]
	add rsp, 10
	ret
