.text
.globl _get_rbp

.type _get_rbp,	@function
_get_rbp:
	mov	%rbp,	%rax
	ret
.size _get_rbp, .-_get_rbp
