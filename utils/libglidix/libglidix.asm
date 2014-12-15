;	Glidix
;
;	Copyright (c) 2014, Madd Games.
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

%macro GLIDIX_SYSCALL 2			; syscall num, syscall name
[global %2]
%2:
	ud2
	dw %1
	ret
%endmacro

GLIDIX_SYSCALL		1,	write
GLIDIX_SYSCALL		2,	_glidix_exec
GLIDIX_SYSCALL		3,	read
GLIDIX_SYSCALL		4,	_glidix_open
GLIDIX_SYSCALL		5,	close
GLIDIX_SYSCALL		6,	getpid
GLIDIX_SYSCALL		7,	getuid
GLIDIX_SYSCALL		8,	geteuid
GLIDIX_SYSCALL		9,	_glidix_getsuid
GLIDIX_SYSCALL		10,	getgid
GLIDIX_SYSCALL		11,	getegid
GLIDIX_SYSCALL		12,	_glidix_getsgid
GLIDIX_SYSCALL		13,	_glidix_sighandler
GLIDIX_SYSCALL		14,	_glidix_sysret
GLIDIX_SYSCALL		15,	stat
GLIDIX_SYSCALL		16,	_glidix_getparsz
GLIDIX_SYSCALL		17,	_glidix_getpars
