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

bits 64

global fpuInit
global fpuSave
global fpuLoad
global fpuGetMXCSR
global fpuSetMXCSR

fpuInit:
	finit
	mov	rax,	cr0
	and	ax,	0xFFFB
	or	ax,	0x2
	mov	cr0,	rax
	mov	rax,	cr4
	or	ax,	(3 << 9)
	mov	cr4,	rax
	
	; flush to zero, mask precision errors
	mov	eax,	(1 << 15) | (1 << 12)
	sub	rsp,	4
	mov	[rsp],	eax
	ldmxcsr	[rsp]
	add	rsp,	4
	ret

fpuSave:
	fxsave	[rdi]
	ret

fpuLoad:
	fxrstor	[rdi]
	ret

fpuGetMXCSR:
	sub	rsp,	4
	stmxcsr	[rsp]
	mov	eax,	[rsp]
	add	rsp,	4
	ret

fpuSetMXCSR:
	sub	rsp,	4
	mov	[rsp],	edi
	ldmxcsr	[rsp]
	add	rsp,	4
	ret
