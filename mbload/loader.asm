global _start
extern stack
extern mb_main
bits 32

MBALIGN  equ  1<<0              ; align loaded modules on page boundaries
MEMINFO  equ  1<<1              ; provide memory map
FLAGS    equ  MBALIGN | MEMINFO ; this is the Multiboot 'flag' field
MAGIC    equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM equ -(MAGIC + FLAGS)   ; checksum of above, to prove we are multiboot

section .multiboot
align 4
	dd MAGIC
	dd FLAGS
	dd CHECKSUM

section .text
_start:
	; set up the stack
	mov esp, stack
	xor ebp, ebp
	
	push ebx		; pass the multiboot structure as an argument
	call mb_main

hang:
	cli
	hlt
	jmp hang

global GDTPointer
global GDT
global GDT_End
GDT:                                 ; Global Descriptor Table (64-bit).
	.Null: equ $ - GDT         ; The null descriptor.
	dw 0                         ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 0                         ; Access.
	db 0                         ; Granularity.
	db 0                         ; Base (high).
	.Code: equ $ - GDT         ; The code descriptor.
	dw 0                         ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 10011000b                 ; Access.
	db 00100000b                 ; Granularity.
	db 0                         ; Base (high).
	.Data: equ $ - GDT         ; The data descriptor.
	dw 0                         ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 10010000b                 ; Access.
	db 00000000b                 ; Granularity.
	db 0                         ; Base (high).
	.UserCode: equ $ - GDT
	dw 0
	dw 0
	db 0
	db 11111000b                 ; Access.
	db 00100000b                 ; Granularity.
	db 0                         ; Base (high).
	.UserData: equ $ - GDT
	dw 0                         ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 11110010b                 ; Access.
	db 00000000b                 ; Granularity.
	db 0                         ; Base (high).
	.UserCode2: equ $ - GDT    ; Second user code segment, for SYSRET
	dw 0
	dw 0
	db 0
	db 11111000b                 ; Access.
	db 00100000b                 ; Granularity.
	db 0                         ; Base (high).
	; The TSS (entry 6)
	.TSS: equ $ - GDT
	.TSS_limitLow: dw 0
	.TSS_baseLow: dw 0
	.TSS_baseMiddle: db 0
	.TSS_Access: db 11101001b
	.TSS_limitHigh: db 0
	.TSS_baseMiddleHigh: db 0
	.TSS_baseHigh: dd 0
	dd 0
GDT_End:

	GDTPointer:
	.Pointer:                    ; The GDT-pointer.
	dw $ - GDT - 1             ; Limit.
	.Addr64: dd GDT                     ; Base.
	dd 0
	
global mb_go64
mb_go64:
	mov esi, [esp+12]

	; load the page table
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
	
	lgdt [GDTPointer]
	jmp 0x08:_entry64

_entry64:
incbin "stub64.bin"
