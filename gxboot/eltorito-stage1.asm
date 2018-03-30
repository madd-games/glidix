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

org 0x6000

jmp boot

times 8-($-$$) db 0
;	Boot Information Table
bi_PrimaryVolumeDescriptor  resd  1    ; LBA of the Primary Volume Descriptor
bi_BootFileLocation         resd  1    ; LBA of the Boot File
bi_BootFileLength           resd  1    ; Length of the boot file in bytes
bi_Checksum                 resd  1    ; 32 bit checksum
bi_Reserved                 resb  40   ; Reserved 'for future standardization'

boot:
; set up segments
cli
xor ax, ax
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x6000

; copy our own code to 0x6000
mov si, 0x7C00
mov di, 0x6000
mov cx, 0x800
rep movsb

; update code segment and IP
jmp 0:start
start:
sti

; the VBR (to be loaded at 0x7C00) is directly after us
mov ax, [bi_BootFileLocation]
inc ax
mov [dap.lba], ax

; number of sectors to read = (bootFileSize - 2048) / 2048 + 1
mov ax, [bi_BootFileLength]
sub ax, 2048
shr ax, 11
inc ax
mov [dap.count], ax

; preserve the boot disk ID
push dx

; read (disk number already in DL)
mov si, dap
mov ah, 0x42
int 0x13
jc boot_failed

; restore disk number and jump to VBR
pop dx
jmp 0:0x7C00

; we jump to here if the boot fails
boot_failed:
int 0x18

dap:
db 0x10				; size
db 0				; unused
.count dw 1			; number of sectors to read
dw 0x7C00			; destination offset
dw 0				; destination segment
.lba dd 0			; LBA to read from
dd 0				; high 32 bits of LBA, unused
