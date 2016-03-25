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

; stage2_pm.asm
; The Protected Mode assembly part of the loader. The main reason for this existing
; is actually so that we can switch back to Real Mode to call the BIOS.

section .text
global bios_call
global jump_to_kernel

bios_call:
	; we must preserve EBP and EBX
	push ebp
	mov ebp, esp
	push ebx
	push esi
	push edi
	
	mov edi, [ebp+8]
	mov esi, [ebp+12]
	
	; jump to 16-bit pmode and let it do the rest (stage2_rm.asm)
	jmp 0x18:0x7C05

jump_to_kernel:
	; multiboot header passed as first argument
	mov ebx, [esp+4]
	
	; kernel entry point passed as second argument
	mov eax, [esp+8]
	
	; jump to kernel
	jmp eax
