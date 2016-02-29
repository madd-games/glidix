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

bits 16
org 0

; stage2.asm - Main body of Glidix bootloader.
; This code is loaded onto the boot partition. The partition is expected
; to be GXFS-formatted; so at offset 16, we have the CIS offset.
; The bootloader requires more than one sector of code to be loaded, so
; we must make sure that we are loaded by a Glidix-compatible MBR.

db 0xEA, 0x18, 0x00, 0xc0, 0x07	; jump to 0x7c0:24 (start)
times 11 db 0			; zero out the next 11 bytes to put cis_offset at +16
cis_offset dq 0			; to be filled in during installation
start:

cli
mov cx, [si+8]
mov di, 0x7c0
mov ds, di
mov ss, di
mov sp, 0x2400

cmp ax, 0xDEAD
jne not_glidix_mbr
cmp bx, 0xBEEF
jne not_glidix_mbr

mov [part_start_lba], cx
mov [boot_drive], dl

mov si, str_welcome
call print

; get drive parameters
push bp
mov [boot_drive], dl
mov ah, 0x08
xor bx, bx
mov es, bx			; must set ES:DI to 0 to work
mov di, bx			; around some buggy BIOSes apparently.
int 0x13
jc get_params_fail				; if CF=1, we fail
pop bp

; DH is the last logical index of heads; add 1 to get the number of heads
inc dh
mov [num_heads], dh

; we need to disentangle last_cylinder and num_sectors from CX!
mov dl, ch			; DL = low 8 bits of last_cylinder
mov dh, cl			; DH = now has bits 8-9 of last_cylinder in its top 2 bits
shr dh, 6
and dh, 3			; now DX = last_cylinder; add 1 to get number of cylinders
inc dx
mov [num_cylinders], dx

; bottom 6 bits of CL contain the "maximum sector number"; since sectors are numbered from 1,
; this is also the number of sectors
and cl, 0x3F
mov [num_sectors], cl

; load the CIS into memory
mov ax, ds
mov es, ax
mov di, cis
mov eax, [cis_offset]
mov ecx, 64
call read_disk

; find the number of sections from inode info
mov eax, [cis.cisTotalIno]
mov edx, [cis.cisTotalIno+4]
mov ebx, [cis.cisInoPerSection]
div ebx
mov [num_sections], eax

mov si, str_num_sections
call print
call printdec
mov si, str_endline
call print

; load the section definitions into the memory below the stack
; (at 0x7c0:0x2400).
; ES already equals DS.
mov ecx, eax
shl ecx, 5			; size = numSections * 32
mov eax, [cis.cisOffSections]
mov di, 0x2400
call read_disk

; put the inode buffer right after the section definition tables
add di, cx
mov [inoBufferLoc], di

mov eax, 2
call read_inode

xor eax, eax
mov ax, [di]
call printdec

cli
hlt

not_glidix_mbr:
mov si, str_not_glidix_mbr
call print
cli
hlt

get_params_fail:
mov si, str_get_params_fail
call print
cli
hlt

%include "display.asm"
%include "gxfs.asm"

;; DATA
str_welcome db '*** GLIDIX LOADER 1.0 ***', 13, 10, 0
str_not_glidix_mbr db 'gxld: I must be loaded by the Glidix MBR!', 13, 10, 0
str_get_params_fail db 'gxld: Failed to get drive paramters from BIOS!', 13, 10, 0
str_num_sections db 'Number of sections on partition: ', 0
str_endline db 13, 10, 0

part_start_lba dw 0
boot_drive db 0

num_heads db 0
num_cylinders dw 0		; yes, cylinders need a 16-bit value, not 8-bit
num_sectors db 0

num_sections dd 0

cis:
.cisMagic dd 0
.cisTotalIno dq 0
.cisInoPerSection dq 0
.cisTotalBlocks dq 0
.cisBlocksPerSection dq 0
.cisBlockSize dw 0
.cisCreateTime dq 0
.cisFirstDataIno dq 0
.cisOffSections dq 0
dw 0				; reserved field

inoBufferLoc dw 0		; inode buffer location
