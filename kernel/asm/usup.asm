;	Glidix kernel
;
;	Copyright (c) 2014-2017, Madd Games.
;	All rights reserved.
;	
;	Redistribution and use in source and binary forms, with or without
;	modification, are permitted provided that the following conditions are met:
;	
;	* Redistributions of source code must retain the above copyright notice, this
;		list of conditions and the following disclaimer.
;	
;	* Redistributions in binary form must reproduce the above copyright notice,
;		this list of conditions and the following disclaimer in the documentation
;		and/or other materials provided with the distribution.
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

section .usup_text
bits 64

global usup_start
global usup_thread_entry
global usup_sigret
global usup_syscall_reset

; code for the user support page. this is a page shared by all processes, containing read-only
; userspace code which helps the kernel manage certain things in userspace. for example, it contains
; code that exits a thread when its entry point function returns.

; mark the beginning of code using this label
usup_start:

;; entry point to a thread
;; kernel passes:
;;	R12 = signal mask
;;	R13 = size of stack (0 if already set)
;;	R14 = entry point
;;	R15 = argument to entry point
usup_thread_entry:
	; map the stack if necessary
	test	r13,	r13
	jz	_stack_done

	mov	rax,	54		; mmap
	mov	rdi,	0		; NULL
	mov	rsi,	r13		; len = stack size
	mov	rdx,	3		; PROT_READ | PROT_WRITE
	mov	r10,	0x15		; MAP_PRIVATE | MAP_ANON | MAP_THREAD
	mov	r8,	-1		; fd = -1
	mov	r9,	0		; offset = 0
	syscall
	
	mov	rsp,	rax
	add	rsp,	r13
	
_stack_done:
	; align the stack to the nearest 16 bytes boundary
	and	rsp,	~0xF
	
	; set the signal mask
	push	r12
	mov	rax,	14	; sigprocmask
	mov	rdi,	2	; SIG_SETMASK
	mov	rsi,	rsp	; pointer to signal mask we just pushed
	mov	rdx,	0	; NULL pointer for old mask
	syscall
	
	; put the errno pointer on the stack
	; we subtract 16 bytes instead of 4 to ensure the stack is still
	; 16-bytes-aligned (and we already subtracted 8 above)
	sub	rsp,	8
	mov	rdi,	rsp
	mov	rax,	49	; _glidix_seterrnoptr()
	syscall
	
	; call the entry point
	mov	rdi,	r15
	call	r14
	
	mov	rdi,	rax	; pass return value as argument to pthread_exit()
	mov	rax,	114	; pthread_exit()
	syscall

;; returning from signal
;; kernel passes:
;;	RBX = pointer to FPU registers
;;	R12 = pointer to GPRs
;;	R13 = old signal mask to restore
usup_sigret:
	; sigmask
	push	r13		; push the signal mask
	mov	rdi,	2	; SIG_SETMASK
	mov	rsi,	rsp	; pointer to the signal mask
	mov	rdx,	0	; NULL pointer for old mask (which we don't care about)
	mov	rax,	14	; sigprocmask()	(multithread-safe under glidix)
	syscall
	
	; restore FPU registers
	fxrstor	[rbx]
	
	; move the stack pointer to where the "rflags" value is
	mov	rsp,	r12
	
	; restore the flags
	popf
	
	; restore all registers except RIP
	mov	rdi,	[rsp+8]
	mov	rsi,	[rsp+16]
	mov	rbp,	[rsp+24]
	mov	rbx,	[rsp+32]
	mov	rdx,	[rsp+40]
	mov	rcx,	[rsp+48]
	mov	rax,	[rsp+56]
	mov	r8,	[rsp+64]
	mov	r9,	[rsp+72]
	mov	r10,	[rsp+80]
	mov	r11,	[rsp+88]
	mov	r12,	[rsp+96]
	mov	r13,	[rsp+104]
	mov	r14,	[rsp+112]
	mov	r15,	[rsp+120]
	
	; we do not restore RSP from the stack, since we know its value relative to current
	; RSP and it wouldn't work anyway
	; there are 16 GPRs to pop off now that they're restored, plus the red zone (128 bytes),
	; each GPR is 8 bytes long.
	; 128 + 16 * 8 = 256
	ret	256

;; resettable system calls
;; kernel passes return RIP in R9, sets up all registers for system calls (RAX, arguments) and transfers
;; control to this code. we invoke the system call and then jump back to the return RIP. note that system
;; calls preserve R9 (just for this purpose). this is used when a signal is dispatched during a system call
;; which cannot return EINTR, and is instead automatically re-tried.
usup_syscall_reset:
	syscall
	jmp	r9
