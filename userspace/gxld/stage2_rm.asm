;	Glidix Loader
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

org 0x7C00
bits 16

db 0xEA, 0x18, 0x7C, 0x00, 0x00	; jump to 0:0x7C18 (start)
db 0xEA
dw api_entry
db 0x18, 0x00
times 6 db 0			; zero out the next 6 bytes to put cis_offset at +16
cis_offset dq 0			; to be filled in during installation
start:

; Get partition start LBA
mov cx, [si+8]

xor ax, ax
mov ds, ax
mov es, ax

xor ax, ax
mov ss, ax
mov sp, 0x7C00

; Enable A20
mov ax, 0x2401
push cx
int 0x15
pop cx

; Disable interrupts before we go to Protected Mode.
cli

; Load GDT
lgdt [GDT32.Pointer]

; Enable Protected Mode.
mov eax, cr0
or eax, 1
mov cr0, eax

; jump to it
jmp 0x08:goto32

api_entry:
	; disable protected mode
	mov eax, cr0
	and eax, ~1
	mov cr0, eax
	
	; now jump to 16-bit code
	jmp 0:entry16

entry16:
	xor ax, ax
	mov ds, ax
	mov ss, ax
	mov es, ax
	sti
	
	; DI stores the interrupt number, SI stores the pointer
	; to the Regs structure.
	
	push si
	mov eax, [si]
	mov ebx, [si+4]
	mov ecx, [si+8]
	mov edx, [si+12]
	push word [si+16]		; push SI
	push word [si+18]		; push DI
	push ax
	mov ax, [si+20]
	mov es, ax
	pop ax
	
	cmp di, 0x10
	je do_int10
	cmp di, 0x13
	je do_int13
	cmp di, 0x15
	je do_int15
	
	cli
	hlt
	
	back_to_pmode:
	pop bp
	mov [bp], eax
	mov [bp+4], ebx
	mov [bp+8], ecx
	mov [bp+12], edx
	mov [bp+16], si
	mov [bp+18], di
	pushf
	pop word [bp+22]
	
	; switch back to protected mode!
	cli
	mov ebx, cr0
	or ebx, 1
	mov cr0, ebx
	
	jmp 0x08:return32

do_int10:
	pop di
	pop si
	int 0x10
	jmp back_to_pmode

do_int13:
	pop di
	pop si
	int 0x13
	jmp back_to_pmode

do_int15:
	pop di
	pop si
	int 0x15
	jmp back_to_pmode
	
bits 32
GDT32:                               ; Global Descriptor Table (32-bit).
	.Null: equ $ - GDT32
	dw 0                    ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 0                 ; Access.
	db 0                 ; Granularity.
	db 0                         ; Base (high).
	.Code: equ $ - GDT32         ; The code descriptor.
	dw 0xFFFF                    ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 10011000b                 ; Access.
	db 11001111b                 ; Granularity.
	db 0                         ; Base (high).
	.Data: equ $ - GDT32         ; The data descriptor.
	dw 0xFFFF                    ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 10010010b                 ; Access.
	db 10001111b                 ; Granularity.
	db 0                         ; Base (high).
	.Code16: equ $ - GDT32
	dw 0xFFFF                    ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 10011000b                 ; Access.
	db 00001111b                 ; Granularity.
	db 0                         ; Base (high).
	.Pointer:                    ; The GDT-pointer.
	dw $ - GDT32 - 1             ; Limit.
	.Addr: dd GDT32                     ; Base.

return32:
	; return from a call to rmode
	mov bx, 0x10
	mov ds, bx
	mov ss, bx
	mov es, bx
	mov fs, bx
	mov gs, bx
	
	pop edi
	pop esi
	pop ebx
	pop ebp
	ret
	
goto32:
mov ax, 0x10
mov ds, ax
mov es, ax
mov ss, ax
mov fs, ax
mov gs, ax

; we are linked such that the rest of 32-bit code comes straight after this.
