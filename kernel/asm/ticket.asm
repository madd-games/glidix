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

global tlInit
global tlLock
global tlUnlock

extern kyield

section .text

tlInit:
	push	rbp
	mov	rbp,		rsp
	
	; set the ticket and exit counter both to zero
	xor	rax,		rax
	mov	[rdi],		rax
	mov	[rdi+8],	rax
	
	pop	rbp
	ret

tlLock:
	push	rbp
	mov	rbp,		rsp
	
	; preserve registers
	sub	rsp,		16
	mov	[rsp],		rbx
	mov	[rsp+8],	r12
	
	; ticket line pointer in RBX
	mov	rbx,		rdi
	
	; get the ticket and store it in R12
	mov	r12,		1
lock	xadd	[rbx],		r12

.repeat:
	; see if it's our turn yet
	mov	rax,		[rbx+8]
	cmp	rax,		r12
	je	.success
	
	; it's not our turn, so yield and try again
	call	kyield
	jmp	.repeat

.success:
	; successfully acquired the lock
	; restore registers and exit
	mov	rbx,		[rsp]
	mov	r12,		[rsp+8]
	add	rsp,		16
	
	pop	rbp
	ret

tlUnlock:
	; increment the exit counter
lock	inc	qword [rdi+8]
	ret
