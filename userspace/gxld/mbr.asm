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
; mbr.asm - Master Boot Record
; We need to find a bootable partition and chainload it, by loading
; the first 8 sectors. Most MBRs just load one sector, but the Glidix
; bootloader may use more. Windows and other OSes will still boot using
; this MBR.

; let's first move ourselves to 0x60:0
cli
cld
mov ax, 0x7c0
mov ds, ax
mov ax, 0x60
mov es, ax
mov cx, 512
xor si, si
xor di, di
rep movsb

; load segment values
jmp 0x60:start
start:
mov ax, 0x60
mov ds, ax
mov ss, ax
mov sp, 0x400

; find a bootable partition
mov bp, 0x1BE
mov cx, 4			; max 4 entries

part_search:
mov al, [bp]
test al, 0x80
jnz found_bootpart		; bootable bit set, we can boot this partition

; else move to the next partition, and try again if we didn't
; yet exhaust the partition table entries
add bp, 16
loop part_search

; none of the partitions are bootable
fail:
int 0x18

found_bootpart:
; DS:BP points to the partition we are booting
; DL is currently set to the boot drive number
; we will be using LBA values from the partition record, because the CHS values
; are not necessarily valid (especially for large disks).
; gxpart doesn't even enter values into the CHS fields!
; so first we need the drive parameters
push bp
mov [boot_drive], dl
mov ah, 0x08
xor bx, bx
mov es, bx			; must set ES:DI to 0 to work
mov di, bx			; around some buggy BIOSes apparently.
int 0x13
jc fail				; if CF=1, we fail
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

; now that we know the number of heads, cylinders and sectors, get the LBA of the partition start.
; the top 16 bits should be clear; let's avoid 32-bit mess in this 16-bit MBR.
mov ax, [bp+10]
test ax, ax
jnz fail
mov ax, [bp+8]

; calculate rel_cylinder = lba / sectors_per_cylinder -> AX
; then we put sector_rel_0 = lba % sectors_per_cylinder -> BX
xor dx, dx
mov bl, [num_sectors]
xor bh, bh
div bx
mov bx, dx			; move the remainder (i.e. CHS sector_rel_0) to BX as promised

; now AX = rel_cylinder, so calculate
; head = rel_cylinder / cylinders_per_head -> AX
; cylinder = rel_cylinder % cylinders_per_head -> DX
xor dx, dx
mov cx, [num_cylinders]
div cx

; OK, BX contains the sector counting from zero, convert it to be 1-based
inc bx

; we now have:
; AL = head
; BL = sector
; DX = cylinder

; in CX, again, we need the top 2 bits (8-9) of the cylinder, then 6 bits of
; the sector.
mov ch, dl
mov cl, dh
shl cl, 6
or cl, bl
mov dh, al		; DH = head
mov ah, 0x02		; read sectors
mov al, 16		; we're reading 16 of them
mov dl,	[boot_drive]

; buffer (ES:BX) at 0x7C0:0
mov bx, 0x7C0
mov es, bx
mov bx, 0

; read the sectors
push bp
int 0x13
jc fail
pop bp

; set up standard registers for second-stage loader
; DS:SI -> partition table entry (also expected by other bootloaders)
; DL = boot drive (also expected by other bootloaders)
; AX = 0xDEAD }
; BX = 0xBEEF } To indicate that it was loaded by a Glidix-compatible MBR.
; IF=1
mov si, bp
mov dl, [boot_drive]
mov ax, 0xDEAD
mov bx, 0xBEEF

; jump to the second-stage loader
sti
jmp 0x7C0:0

; -- DATA AREA --
boot_drive db 0
num_heads db 0
num_cylinders dw 0		; yes, cylinders need a 16-bit value, not 8-bit
num_sectors db 0
