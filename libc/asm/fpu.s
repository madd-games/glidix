.text
.globl __init_fpuregs

.type __init_fpuregs,	@function
__init_fpuregs:
	subq		$4,		%rsp
	movl		$0x9000,	%eax
	movl		%eax,		(%rsp)
	ldmxcsr		(%rbp)
	addq		$4,		%rsp
	ret
.size __init_fpuregs, .-__init_fpuregs
