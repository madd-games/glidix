	.file	"test.c"
	.section	.rodata
.LC3:
	.string	"i memed"
	.text
	.globl	main
	.type	main, @function
main:
.LFB2:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	subq	$16, %rsp
	movl	.LC0(%rip), %eax
	movl	%eax, -12(%rbp)
	movl	.LC1(%rip), %eax
	movl	%eax, -8(%rbp)
	movss	-12(%rbp), %xmm1
	cvtps2pd	%xmm1, %xmm1
	movss	-8(%rbp), %xmm0
	cvtps2pd	%xmm0, %xmm0
	movsd	.LC2(%rip), %xmm2
	divsd	%xmm2, %xmm0
	addsd	%xmm1, %xmm0
	unpcklpd	%xmm0, %xmm0
	cvtpd2ps	%xmm0, %xmm3
	movss	%xmm3, -4(%rbp)
	movl	$.LC3, %edi
	call	puts
	movl	$0, %eax
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE2:
	.size	main, .-main
	.section	.rodata
	.align 4
.LC0:
	.long	1056964608
	.align 4
.LC1:
	.long	1053609165
	.align 8
.LC2:
	.long	2576980378
	.long	1070176665
	.ident	"GCC: (Ubuntu 4.8.4-2ubuntu1~14.04) 4.8.4"
	.section	.note.GNU-stack,"",@progbits
