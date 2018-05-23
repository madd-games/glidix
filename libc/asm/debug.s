.text
.globl __stack_trace

.type __stack_trace,	@function
__stack_trace:
	pushq		%rbp
	mov		%rsp, %rdi
	call __print_trace@PLT
	pop		%rbp
	ret
.size __stack_trace, .-__stack_trace
