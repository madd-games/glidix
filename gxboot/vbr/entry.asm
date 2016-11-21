extern vbr_end
extern bmain

global boot_disk
global part_start
global _start

section .entry_text
bits 16

_start:
cli
xchg bx, bx

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

; enable A20
mov ax, 0x2401
int 0x15

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

boot_failed:
int 0x18

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

goto32:
mov ax, 0x10
mov ds, ax
mov es, ax
mov ss, ax
mov fs, ax
mov gs, ax

mov esp, 0x7C00
call bmain

crash_loop:
cli
hlt
jmp crash_loop
