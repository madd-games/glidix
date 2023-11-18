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

; disable interrupts before doing anything
cli

; set up segments and the stack
xor ax, ax
mov ds, ax
mov es, ax
mov ss, ax
mov sp, 0x6000

; copy the code to 0x6000
mov si, 0x7C00
mov di, 0x6000
mov cx, 512
cld
rep movsb

; fix the instruction pointer
jmp 0:start
start:

; stack is set up, enable interrupts
sti

; preserve boot disk number
mov [boot_drive], dl

; make sure INT13 extensions are present
mov ah, 0x41
; drive number already in DL
mov bx, 0x55AA
int 0x13
jc err_no_int13_ext

; test BX and whether reads are supported
cmp bx, 0xAA55
jne err_no_int13_ext
test cx, 1
jz err_no_int13_ext

; load the GPT header (LBA 1)
mov eax, 1
xor ebx, ebx
mov cx, 1
call read_sectors

; check that the signature is correct
mov si, 0x7C00
mov di, gpt_sig
mov cx, 8
call memcmp
jnz err_bad_gpt

; check that the size of a single partition entry is 128 bytes
cmp [0x7C54], dword 128
jne err_bad_gpt

; check that the partition table begins at LBA 2
cmp [0x7C48], dword 2
jne err_bad_gpt

; get the number of partition table entries
mov ecx, [0x7C50]
test ecx, ecx
jz err_bad_gpt

; if too many entries, truncate to a maximum
cmp ecx, 256
jl part_count_ok
mov ecx, 256
part_count_ok:

; save the number of partitions to the stack
push cx

; read the partition table
mov eax, 2
xor ebx, ebx
shr cx, 2
inc cx
call read_sectors

; pop the number of partitions into dx
pop dx

; search for the stage2 partition
mov si, 0x7C00
mov di, stage2_type_guid

iter_parts:
mov cx, 16
call memcmp
jz part_found

dec dx
test dx, dx
jz err_no_stage2

add si, 128
jmp iter_parts

; we get here once the partition was found and 'si' points to the partition table entry.
; copy the partition table entry to 0x6200.
part_found:
mov di, 0x6200
mov cx, 128
rep movsb
sub si, 128

; load 64KB (128 sectors) from the partition, regardless of the size. the bootloader is
; assumed to fit in 64KB (that's the most we can read anyway), even if it has extra data.
; if the partition is smaller than that, then the bootloader is padded with garbage which
; is also OK
mov eax, [si+32]
mov ebx, [si+36]
mov cx, 128
call read_sectors

; load the boot drive number back into DL, and jump to the second stage
; we guarantee that the segment:offset address will be [0:0x7C00] (unlike BIOS which sometimes
; uses [0x7C0:0]).
; pass the address of the partition table entry in 'si'.
mov dl, [boot_drive]
mov si, 0x6200
jmp 0:0x7C00

; PROCEDURE err_no_int13_ext
; Called when INT13 extensions are not present. Can be called as a function,
; or via a simple jump. Displays an error message and crashes.
err_no_int13_ext:
	mov si, str_no_int13_ext
	call crash

; PROCEDURE strlen
; Input: SI = pointer to string (NUL-terminated)
; Output: CX = number of bytes in string (does not include NUL)
; Preserves all registers except CX
strlen:
	push si
	push ax
	xor cx, cx
	.repeat:
	lodsb
	test al, al
	jz .finish
	inc cx
	jmp .repeat
	.finish:
	pop ax
	pop si
	ret
	
; PROCEDURE crash
; Input: SI = pointer to error string to display
; This procedure never returns; you can call it either with an actual call,
; or with a simple jump.
crash:
	push si
	mov ah, 0x03
	xor bh, bh
	int 0x10
	pop si
	
	mov ax, 0x1301
	mov bx, 0x04
	mov bp, si
	call strlen
	int 0x10
	
	.stop:
	cli
	hlt
	jmp .stop

; PROCEDURE read_sectors
; Input:
;	EAX = low 32 bits of the LBA
;	EBX = high 32 bits of the LBA
;	CX = number of sectors to read
; Reads sectors from disk and writes them to 0x7C00.
; Trashes all registers.
read_sectors:
	mov [dap.count], cx
	mov [dap.lba], eax
	mov [dap.lba+4], ebx
	
	mov ah, 0x42
	mov dl, [boot_drive]
	mov si, dap
	int 0x13
	jc err_read
	
	ret

; PROCEDURE err_read
; Called when a read error occurs. Can be called either as a function or using
; a simple jump; prints an error message and crashes.
err_read:
	mov si, str_read_error
	call crash

; PROCEDURE memcmp
; Input:
;	SI = memory block A
;	DI = memory block B
;	CX = size of the blocks
; Output:
;	ZF=1 if same, else ZF=0
; Trashes: CX
memcmp:
	push si
	push di
	dec cx
	.repeat:
	cmpsb
	loopz .repeat
	pop di
	pop si
	ret

; PROCEDURE err_bad_gpt
; Called when the GPT is invalid. Can be called either as a function or using
; a simple jump; prints an error message and crashes.
err_bad_gpt:
	mov si, str_bad_gpt
	call crash

; PROCEDURE err_no_stage2
; Called when no stage2 partition exists. Can be called either as a function or using
; a simple jump; prints an error message and crashes.
err_no_stage2:
	mov si, str_no_stage2
	call crash

;; === DATA ===
boot_drive db 0

str_no_int13_ext db 'E: INT13 ext not supported', 0
str_read_error db 'E: Read error', 0
str_bad_gpt db 'E: Bad GPT', 0
str_no_stage2 db 'E: No stage2 partition', 0

gpt_sig db 'EFI PART'

stage2_type_guid db 0x9C, 0xAD, 0xC1, 0x81, 0xC4, 0xBD, 0x09, 0x48, 0x8D, 0x9F, 0xDC, 0xB2, 0xA9, 0xB8, 0x5D, 0x01

dap:
db 0x10				; size
db 0				; unused
.count dw 0			; number of sectors to read
dw 0				; destination offset
dw 0x7C0			; destination segment
.lba:				; LBA to read from
