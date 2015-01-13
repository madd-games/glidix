;	Glidix
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

%macro GLIDIX_SYSCALL 2			; syscall num, syscall name
[global %2]
%2:
	xor rax, rax
	ud2
	dw %1
	ret
%endmacro

global _glidix_pollpid
_glidix_pollpid:
	; this syscall is special because it returns the status is RDI!
	xor rax, rax
	ud2
	dw 25
	test rsi, rsi
	jz .nopass
	mov [rsi], edi		; (int = 32 bits)
	.nopass:
	ret

GLIDIX_SYSCALL		0,	_exit
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
GLIDIX_SYSCALL		14,	_glidix_sigret
GLIDIX_SYSCALL		15,	stat
GLIDIX_SYSCALL		16,	_glidix_getparsz
GLIDIX_SYSCALL		17,	_glidix_getpars
GLIDIX_SYSCALL		18,	raise
GLIDIX_SYSCALL		19,	_glidix_geterrno
GLIDIX_SYSCALL		20,	_glidix_seterrno
GLIDIX_SYSCALL		21,	mprotect
GLIDIX_SYSCALL		22,	lseek
GLIDIX_SYSCALL		23,	_glidix_clone
GLIDIX_SYSCALL		24,	pause
; system call 25 handled in a special way; see above.
GLIDIX_SYSCALL		26,	kill
GLIDIX_SYSCALL		27,	_glidix_insmod
GLIDIX_SYSCALL		28,	_glidix_ioctl
GLIDIX_SYSCALL		29,	_glidix_fdopendir
GLIDIX_SYSCALL		30,	_glidix_diag
GLIDIX_SYSCALL		31,	_glidix_mount
GLIDIX_SYSCALL		32,	_glidix_yield
GLIDIX_SYSCALL		33,	time
GLIDIX_SYSCALL		34,	realpath
GLIDIX_SYSCALL		35,	chdir
GLIDIX_SYSCALL		36,	getcwd
GLIDIX_SYSCALL		37,	fstat
GLIDIX_SYSCALL		38,	chmod
GLIDIX_SYSCALL		39,	fchmod
GLIDIX_SYSCALL		40,	fsync
GLIDIX_SYSCALL		41,	chown
GLIDIX_SYSCALL		42,	fchown
GLIDIX_SYSCALL		43,	mkdir
GLIDIX_SYSCALL		44,	ftruncate
GLIDIX_SYSCALL		45,	unlink
