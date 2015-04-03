;	Glidix kernel
;
;	Copyright (c) 2014-2015, Madd Games.
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

section .text
bits 64

[global spinlockAcquire]
[global spinlockTry]
[global spinlockRelease]

spinlockAcquire:
	push	rbp
	mov	rbp,	rsp

.acq:
	mov	al,	1
	xchg	al,	[rdi]
	test	al,	al
	jnz	.acq

	pop	rbp
	ret

spinlockTry:
	push	rbp
	mov	rbp,	rsp

	xor	rax,	rax
	mov	al,	1
	xchg	al,	[rdi]

	pop	rbp
	ret

spinlockRelease:
	push	rbp
	mov	rbp,	rsp

	xor	rax,	rax
	xchg	al,	[rdi]

	; we don't want to starve other processes of resources, so we must reschedule whenever this function
	; is called, in case spinlockAcquire() is called straight afterwards. But obviously, we only want to
	; do it if IF=1, because if interrupts are disabled then we are already in a critical section, and also
	; the scheduler may not be initialized (interrupts are ony enabled once initSched() is called).
	pushfq
	pop	rax
	test	rax,	(1 << 9)		; test the IF bit.
	jz	.end				; interrupts are disabled.

	; interrupts are enabled, so halt and wait for a reschedule.
	hlt

.end:
	pop	rbp
	ret
