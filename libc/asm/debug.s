.globl __sf_current
.type __sf_current, @function

__sf_current:
	movq	%rbp,	%rax
	retq
.size __sf_current, .-__sf_current
