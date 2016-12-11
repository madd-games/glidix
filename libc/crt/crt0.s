.text
.globl _start
.weak __call_init_array

_start:
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
