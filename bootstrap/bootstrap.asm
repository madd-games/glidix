section .bootstrap32
bits 32

; multiboot header
dd 0x1BADB002
dd 65539
dd -(0x1BADB002 + 65539)
dd 0x10000			; base addr
dd 0x10000			; phys base addr
dd 0
dd 0
dd _start

[global _start]
_start:
	cli

	mov edi, 0xB8000
	mov al, 0x69
	mov ecx, 2*80*25
	rep stosb

	hlt
