.text
.globl setjmp
.globl longjmp

.type setjmp, @function
setjmp:
	// NOTE: The stack is preserved in the state that enables a direct 'ret'
	mov	%rbx,		(%rdi)
	mov	%rsp,		8(%rdi)
	mov	%rbp,		16(%rdi)
	mov	%r12,		24(%rdi)
	mov	%r13,		32(%rdi)
	mov	%r14,		40(%rdi)
	mov	%r15,		48(%rdi)
	xor	%rax,		%rax
	ret
.size setjmp, .-setjmp

.type longjmp, @function
longjmp:
	// if (value == 0) value = 1;
	test	%rsi,		%rsi
	jnz __longjmp_ok
	mov	$1,		%rsi
__longjmp_ok:
	mov	(%rdi),		%rbx
	mov	8(%rdi),	%rsp
	mov	16(%rdi),	%rbp
	mov	24(%rdi),	%r12
	mov	32(%rdi),	%r13
	mov	40(%rdi),	%r14
	mov	48(%rdi),	%r15
	mov	%rsi,		%rax
	
	// remember that the value of RSP was set by setjmp() such that we can now just 'ret'
	// and we'll be back in the right place
	ret
.size longjmp, .-longjmp
