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

; display.asm
; screen functions and stuff

; Function print
; Input:
;	DS:SI = points to NUL-terminated string to print
; Output:
;	None.
; Trashes:
;	None.
print:
	pusha
	.repeat:
	lodsb
	test al, al
	jz .end
	
	push si
	mov ah, 0x0E
	xor bh, bh
	mov bl, 0x07
	int 0x10
	pop si
	jmp .repeat
	
	.end:
	popa
	ret

; Function printdec
; Prints a decimal number.
; Input:
;	EAX = number to print
; Output:
;	None
; Trashes:
;	None
printdec:
	pusha
	mov bp, sp
	mov si, sp
	xor dl, dl
	dec si
	mov [si], dl
	mov ebx, 10
	.repeat:
	xor edx, edx
	div ebx
	add dl, '0'
	dec si
	mov [si], dl
	test eax, eax
	jnz .repeat
	
	mov sp, si
	call print
	
	mov sp, bp
	popa
	ret
