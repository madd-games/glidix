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

; entry code for GXFS boot

extern vbr_end
extern bmain

global boot_disk
global part_start
global _start
global biosRead
global dap
global sectorBuffer
global biosGetMap
global go64
global vbeInfoBlock
global vbeModeInfo
global vbeGetModeInfo
global vbeSwitchMode

section .entry_text
bits 16

%ifdef GXBOOT_FS_GXFS
_start:
cli

; figure out the size in sectors
mov eax, [size]
shr eax, 9
inc eax
mov [dap.count], ax

; load starting from the second sector
mov eax, [si+8]
mov [part_start], eax
inc eax
mov [dap.lba], eax

; save the disk number
mov [boot_disk], dl

; set up a new stack
mov sp, 0x7C00

; enable interrupts now
sti

; load the rest of the VBR
mov ah, 0x42
mov si, dap
int 0x13
jc boot_failed
%endif

; this is the starting point for El Torito. the first stage already loaded the whole VBR,
; now we just need to go to protected mode and stuff. but we do still need to switch the stack
; and save the disk number
%ifdef GXBOOT_FS_ELTORITO
mov [boot_disk], dl
mov sp, 0x7C00
%endif

; enable A20
mov ax, 0x2401
int 0x15

; get VBE information
mov ax, 0x4F00
mov di, vbeInfoBlock
int 0x10

test ah, ah
jnz boot_failed

; disable interrupts again, we're going to PMode
cli

; load GDT
lgdt [GDT32.Pointer]

; enable protected mode
mov eax, cr0
or eax, 1
mov cr0, eax

; jump to 32-bit segment
jmp 0x08:goto32

size dd vbr_end - 0x7C00

boot_failed:
int 0x18

dap:
db 0x10				; size
db 0				; unused
.count dw 0			; number of sectors to read
.offset dw 0x7E00		; destination offset (512 bytes past first sector)
dw 0				; destination segment
.lba dd 0			; LBA to read from
dd 0				; high 32 bits of LBA, unused

boot_disk db 0
part_start dd 0

vbeInfoBlock:
db 'VBE2'			; needed, apparently?
resb 512

vbeModeInfo:
resb 256

_biosRead_rm:
	; disable protected mode
	mov eax, cr0
	and eax, ~1
	mov cr0, eax
	
	; go to real mode
	jmp 0:_biosRead_switch

_biosRead_switch:
	; load real mode segments
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	
	; call the BIOS
	sti
	mov ah, 0x42
	mov dl, [boot_disk]
	mov si, dap
	int 0x13
	jc boot_failed
	cli
	
	; enable protected mode again
	mov eax, cr0
	or eax, 1
	mov cr0, eax
	
	; jump to the protected mode part
	jmp 0x08:_biosRead_pm

_vbeGetModeInfo_rm:
	; disable protected mode
	mov eax, cr0
	and eax, ~1
	mov cr0, eax
	
	; go to real mode
	jmp 0:_vbeGetModeInfo_switch

_vbeGetModeInfo_switch:
	; load real mode segments
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	
	; call the BIOS
	sti
	mov ax, 0x4F01
	; mode already in CX
	mov di, vbeModeInfo
	int 0x10
	xor ecx, ecx
	mov cl, ah			; preserve return value in CL
	cli
	
	; enable protected mode again
	mov eax, cr0
	or eax, 1
	mov cr0, eax
	
	; jump to the protected mode part
	jmp 0x08:_vbeGetModeInfo_pm

_vbeSwitchMode_rm:
	; disable protected mode
	mov eax, cr0
	and eax, ~1
	mov cr0, eax
	
	; go to real mode
	jmp 0:_vbeSwitchMode_switch

_vbeSwitchMode_switch:
	; load real mode segments
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	
	; call the BIOS
	sti
	mov ax, 0x4F02
	mov bx, cx
	or bx, (1 << 14)
	int 0x10
	xor ecx, ecx
	mov cl, ah			; preserve return value in CL
	cli
	
	; enable protected mode again
	mov eax, cr0
	or eax, 1
	mov cr0, eax
	
	; jump to the protected mode part
	jmp 0x08:_vbeSwitchMode_pm

_biosGetMap_rm:
	; disable protected mode
	mov eax, cr0
	and eax, ~1
	mov cr0, eax
	
	; go to real mode
	jmp 0:_biosGetMap_switch

_biosGetMap_switch:
	; load real mode segments
	xor ax, ax
	mov ds, ax
	mov es, ax
	mov ss, ax
	
	; preserve destination, ok pointer
	push dx
	
	; call the BIOS
	sti
	mov di, mmapBuffer
	mov edx, 0x534D4150
	mov eax, 0xE820
	mov ecx, 24
	int 0x15
	cli
	
	; if carry flag is set, we change "ok" to 0 and return whatever,
	; otherwise just normally return to protected mode
	pop di
	jnc .no_carry
	xor eax, eax
	mov [di], eax
	
.no_carry:
	mov eax, ebx
	
	; enable protected mode again
	mov ecx, cr0
	or ecx, 1
	mov cr0, ecx
	
	; jump back to the protected mode part
	jmp 0x08:_biosGetMap_pm

mmapBuffer:
times 24 db 0

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
	.Code64: equ $ - GDT32         ; The code descriptor.
	dw 0                         ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 10011000b                 ; Access.
	db 00100000b                 ; Granularity.
	db 0                         ; Base (high).
	.Data64: equ $ - GDT32         ; The data descriptor.
	dw 0                         ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 10010000b                 ; Access.
	db 00000000b                 ; Granularity.
	db 0                         ; Base (high).
	.Pointer:                    ; The GDT-pointer.
	dw $ - GDT32 - 1             ; Limit.
	.Addr: dd GDT32                     ; Base.

goto32:
mov ax, 0x10
mov ds, ax
mov es, ax
mov ss, ax
mov fs, ax
mov gs, ax

mov esp, 0x7C00
cld
call bmain

crash_loop:
cli
hlt
jmp crash_loop

sectorBuffer:
times 4096 db 0

biosRead:
	; preserve registers
	push ebp
	push ebx
	push esi
	push edi
	
	; go to 16-bit protected mode
	jmp 0x18:_biosRead_rm
	
_biosRead_pm:
	; load protected mode segments
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	
	; restore registers
	pop edi
	pop esi
	pop ebx
	pop ebp
	
	; return
	cld
	ret

_vbeGetModeInfo_pm:
	; load protected mode segments
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	
	; restore registers
	pop edi
	pop esi
	pop ebx
	pop ebp
	
	; restore the VBE return value in EAX
	mov eax, ecx
	
	; return
	cld
	ret

_vbeSwitchMode_pm:
	; load protected mode segments
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	
	; restore registers
	pop edi
	pop esi
	pop ebx
	pop ebp
	
	; restore the VBE return value in EAX
	mov eax, ecx
	
	; return
	cld
	ret

biosGetMap:
	push ebp
	mov ebp, esp
	push ebx
	push esi
	push edi
	
	mov ebx, [ebp+8]		; index into EBX
	mov esi, [ebp+12]		; destination into ESI
	mov edx, [ebp+16]		; ok pointer into EDX
	push esi
	
	; go to 16-bit protected mode
	jmp 0x18:_biosGetMap_rm

vbeGetModeInfo:
	push ebp
	mov ebp, esp
	push ebx
	push esi
	push edi
	
	mov ecx, [ebp+8]		; mode into ECX
	
	; go to 16-bit protected mode
	jmp 0x18:_vbeGetModeInfo_rm

vbeSwitchMode:
	push ebp
	mov ebp, esp
	push ebx
	push esi
	push edi
	
	mov ecx, [ebp+8]		; mode into ECX (for now)
	
	; go to 16-bit protected mode
	jmp 0x18:_vbeSwitchMode_rm
	
_biosGetMap_pm:
	; restore 32-bit segments
	mov bx, 0x10
	mov ds, bx
	mov ss, bx
	mov es, bx
	mov fs, bx
	mov gs, bx
	
	pop edi
	mov esi, mmapBuffer
	mov ecx, 20
	rep movsb
	
	pop edi
	pop esi
	pop ebx
	pop ebp
	cld
	ret

go64:
	; edit segment selector 0x08 to be 64-bit
	mov esi, GDT32 + 0x20
	mov edi, GDT32 + 0x08
	mov ecx, 8
	rep movsb
	
	mov esi, [esp+4]
	
	; load the PML4
	mov eax, [esi+0x18]
	mov cr3, eax

	; enable PAE
	mov eax, cr4
	or eax, 1 << 5
	mov cr4, eax

	; enable long mode
	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	wrmsr

	; enable paging and write-protect
	mov eax, cr0
	or eax, 1 << 31
	or eax, 1 << 16
	mov cr0, eax
	
	; go to 64-bit mode
	jmp 0x08:_entry64

_entry64:
incbin "stub64.bin"
