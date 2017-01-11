;	Glidix bootloader (gxboot)
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

bits 16
org 0x6000

; since we're messing with the stack we probably want to be careful
cli

; reset segment registers
xor ax, ax
mov ds, ax
mov ss, ax
mov es, ax

; relocate code from 0x7C00 to 0x6000
mov si, 0x7C00
mov di, 0x6000
mov cx, 512
cld
rep movsb

; set CS
jmp 0:start
start:

; set up the stack above the code
mov sp, 0x6000

; we can safely enable interrupts now
sti

; preserve boot device on the stack
push dx

; make sure INT13 extensions are present
mov ah, 0x41
; drive number already in DL
mov bx, 0x55AA
int 0x13
jc boot_failed

; test BX and whether reads are supported
cmp bx, 0xAA55
jne boot_failed
test cx, 1
jz boot_failed

; loop through partitions, find the active one
mov si, 0x61BE
mov cx, 4

part_search:
mov al, [si]
cmp al, 0x80
je part_found

add si, 16
loop part_search
jmp boot_failed			; no boot partition found

part_found:
; DS:SI now points to a bootable partition
mov eax, [si+8]
mov [dap.lba], eax
xor eax, eax

; restore drive number from stack, preserve SI
pop dx
push dx
push si
mov si, dap
mov ah, 0x42
int 0x13
jc boot_failed

; restore SI (pointer to partition entry) and drive number
pop si
pop dx

; jump to VBR
jmp 0:0x7C00

boot_failed:
int 0x18

dap:
db 0x10				; size
db 0				; unused
dw 1				; number of sectors to read
dw 0x7C00			; destination offset
dw 0				; destination segment
.lba dd 0			; LBA to read from
dd 0				; high 32 bits of LBA, unused
