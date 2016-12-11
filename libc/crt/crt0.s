.text
.globl _start
.weak __call_init_array

__default_mxcsr:
	.quad (1 << 15) | (1 << 12)

_start:
	ldmxcsr __default_mxcsr
	call _init
	call __call_init_array
	
	mov (%rsp), %rdi
	lea 8(%rsp), %rsi
	
	lea 1(%rdi), %rdx
	shl $3, %rdx
	add %rsi, %rdx
	call main
	
	mov %rax, %rdi
	call exit

__call_init_array:
	ret
