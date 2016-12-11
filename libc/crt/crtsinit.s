.text
.globl __call_init_array

__call_init_array:
	mov $__init_array_start, %rbx
.repeat:
	mov (%rbx), %rax
	test %rax, %rax
	jz .end
	
	call *%rax
	add $8, %rbx
	jmp .repeat

.end:
	ret
