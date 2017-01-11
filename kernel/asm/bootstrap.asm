;	Glidix kernel
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

section .data

global GDTPointer
global GDT64
GDT64:                               ; Global Descriptor Table (64-bit).
	.Null: equ $ - GDT64         ; The null descriptor.
	dw 0                         ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 0                         ; Access.
	db 0                         ; Granularity.
	db 0                         ; Base (high).
	.Code: equ $ - GDT64         ; The code descriptor.
	dw 0                         ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 10011000b                 ; Access.
	db 00100000b                 ; Granularity.
	db 0                         ; Base (high).
	.Data: equ $ - GDT64         ; The data descriptor.
	dw 0                         ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 10010000b                 ; Access.
	db 00000000b                 ; Granularity.
	db 0                         ; Base (high).
	.UserCode: equ $ - GDT64
	dw 0
	dw 0
	db 0
	db 11111000b                 ; Access.
	db 00100000b                 ; Granularity.
	db 0                         ; Base (high).
	.UserData: equ $ - GDT64
	dw 0                         ; Limit (low).
	dw 0                         ; Base (low).
	db 0                         ; Base (middle)
	db 11110010b                 ; Access.
	db 00000000b                 ; Granularity.
	db 0                         ; Base (high).
	.UserCode2: equ $ - GDT64    ; Second user code segment, for SYSRET
	dw 0
	dw 0
	db 0
	db 11111000b                 ; Access.
	db 00100000b                 ; Granularity.
	db 0                         ; Base (high).
	; The TSS (entry 6)
	.TSS: equ $ - GDT64
	.TSS_limitLow: dw 0
	.TSS_baseLow: dw 0
	.TSS_baseMiddle: db 0
	.TSS_Access: db 11101001b
	.TSS_limitHigh: db 0
	.TSS_baseMiddleHigh: db 0
	.TSS_baseHigh: dd 0
	dd 0
	GDTPointer:
	.Pointer:                    ; The GDT-pointer.
	dw $ - GDT64 - 1             ; Limit.
	.Addr64: dq GDT64                     ; Base.

bits 64
global _tss
_tss:
resb 192
_tss_limit:

[global _tss_reload_access]
[extern localGDTPtr]
_tss_reload_access:
	mov al,		11101001b
	;mov rdi,	qword GDT64.TSS_Access
	mov rdi,	qword localGDTPtr
	mov rdi,	[rdi]
	add rdi,	0x35
	stosb
	ret

[global tssPrepare]
tssPrepare:
	mov rax,			qword _tss
	mov rdi,			qword GDT64.TSS_baseLow
	stosw
	shr rax,			16
	mov rdi,			qword GDT64.TSS_baseMiddle
	stosb
	mov rdi,			qword GDT64.TSS_baseMiddleHigh
	shr ax,				8
	stosb
	mov rdi,			qword GDT64.TSS_baseHigh
	shr rax,			16
	stosd

	mov rax,			qword _tss_limit
	mov rdx,			qword _tss
	sub rax,			rdx
	mov rdi,			qword GDT64.TSS_limitLow
	stosw
	shr rax,			16
	or  rax,			(1 << 5)
	mov rdi,			qword GDT64.TSS_limitHigh
	stosw

	ret

section .stack
resb 0x200000
