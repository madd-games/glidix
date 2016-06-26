	.globl		ddiBlendPixel
	.type		ddiBlendPixel, @function
ddiBlendPixel:
.LFB14:
	.cfi_startproc
	pushq		%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq		%rsp, %rbp
	.cfi_def_cfa_register 6
	xchg		%bx, %bx
	movl		%edx, %r8d
	movslq		%r8d, %rdx
	movq		%rdi, %rax
	addq		%rdx, %rax
	movzbl		(%rax), %eax
	movzbl		%al, %eax
	movl		%eax, -4(%rbp)
	movl		%r8d, %eax
	movslq		%eax, %rdx
	movq		%rsi, %rax
	addq		%rdx, %rax
	movzbl		(%rax), %eax
	movzbl		%al, %eax
	movl		%eax, -8(%rbp)
	movl		$255, %eax
	subl		-4(%rbp), %eax
	imull		-8(%rbp), %eax
	movslq		%eax, %rdx
	imulq		$-2139062143, %rdx, %rdx
	shrq		$32, %rdx
	addl		%eax, %edx
	sarl		$7, %edx
	sarl		$31, %eax
	subl		%eax, %edx
	movl		-4(%rbp), %eax
	addl		%edx, %eax
	movl		%eax, -12(%rbp)
	cmpl		$0, -12(%rbp)
	jne		.L114
	movq		%rsi, %rax
	movl		$0, (%rax)
	jmp		.L113
.L114:
	movq		%rsi, %rax
	movzbl		(%rax), %eax
	movzbl		%al, %eax
	movl		%eax, -88(%rbp)
	movq		%rsi, %rax
	addq		$1, %rax
	movzbl		(%rax), %eax
	movzbl		%al, %eax
	movl		%eax, -92(%rbp)
	movq		%rsi, %rax
	addq		$2, %rax
	movzbl		(%rax), %eax
	movzbl		%al, %eax
	movl		%eax, -96(%rbp)
	movq		%rsi, %rax
	addq		$3, %rax
	movzbl		(%rax), %eax
	movzbl		%al, %eax
	movl		%eax, -100(%rbp)
	movd		-96(%rbp), %xmm0
	movd		-100(%rbp), %xmm5
	punpckldq	%xmm5, %xmm0
	movd		-88(%rbp), %xmm1
	movd		-92(%rbp), %xmm6
	punpckldq	%xmm6, %xmm1
	movdqa		%xmm1, %xmm4
	punpcklqdq	%xmm0, %xmm4
	movdqa		%xmm4, %xmm0
	movaps		%xmm0, -32(%rbp)
	movq		%rdi, %rax
	movzbl		(%rax), %eax
	movzbl		%al, %eax
	movl		%eax, -88(%rbp)
	movq		%rdi, %rax
	addq		$1, %rax
	movzbl		(%rax), %eax
	movzbl		%al, %eax
	movl		%eax, -92(%rbp)
	movq		%rdi, %rax
	addq		$2, %rax
	movzbl		(%rax), %eax
	movzbl		%al, %eax
	movl		%eax, -96(%rbp)
	movq		%rdi, %rax
	addq		$3, %rax
	movzbl		(%rax), %eax
	movzbl		%al, %eax
	movl		%eax, -100(%rbp)
	movd		-96(%rbp), %xmm0
	movd		-100(%rbp), %xmm7
	punpckldq	%xmm7, %xmm0
	movd		-88(%rbp), %xmm1
	movd		-92(%rbp), %xmm5
	punpckldq	%xmm5, %xmm1
	movdqa		%xmm1, %xmm5
	punpcklqdq	%xmm0, %xmm5
	movdqa		%xmm5, %xmm0
	movaps		%xmm0, -48(%rbp)
	movl		-4(%rbp), %eax
	movl		%eax, -88(%rbp)
	movd		-88(%rbp), %xmm6
	pshufd		$0, %xmm6, %xmm0
	movdqa		%xmm0, %xmm2
	pmuludq		-48(%rbp), %xmm2
	movdqa		%xmm0, %xmm1
	psrlq		$32, %xmm1
	movdqa		-48(%rbp), %xmm0
	psrlq		$32, %xmm0
	pmuludq		%xmm0, %xmm1
	pshufd		$8, %xmm2, %xmm0
	pshufd		$8, %xmm1, %xmm1
	punpckldq	%xmm1, %xmm0
	movd		%xmm0, %eax
	movslq		%eax, %rdx
	imulq		$-2139062143, %rdx, %rdx
	shrq		$32, %rdx
	addl		%eax, %edx
	sarl		$7, %edx
	sarl		$31, %eax
	subl		%eax, %edx
	movl		%edx, -88(%rbp)
	pshufd		$85, %xmm0, %xmm1
	movd		%xmm1, %eax
	movslq		%eax, %rdx
	imulq		$-2139062143, %rdx, %rdx
	shrq		$32, %rdx
	addl		%eax, %edx
	sarl		$7, %edx
	sarl		$31, %eax
	subl		%eax, %edx
	movl		%edx, -92(%rbp)
	movdqa		%xmm0, %xmm1
	punpckhdq	%xmm0, %xmm1
	movd		%xmm1, %eax
	movslq		%eax, %rdx
	imulq		$-2139062143, %rdx, %rdx
	shrq		$32, %rdx
	addl		%eax, %edx
	sarl		$7, %edx
	sarl		$31, %eax
	subl		%eax, %edx
	movl		%edx, -96(%rbp)
	pshufd		$255, %xmm0, %xmm0
	movd		%xmm0, %eax
	movslq		%eax, %rdx
	imulq		$-2139062143, %rdx, %rdx
	shrq		$32, %rdx
	addl		%eax, %edx
	sarl		$7, %edx
	sarl		$31, %eax
	subl		%eax, %edx
	movl		%edx, -100(%rbp)
	movd		-96(%rbp), %xmm0
	movd		-100(%rbp), %xmm7
	punpckldq	%xmm7, %xmm0
	movd		-88(%rbp), %xmm1
	movd		-92(%rbp), %xmm5
	punpckldq	%xmm5, %xmm1
	movdqa		%xmm1, %xmm6
	punpcklqdq	%xmm0, %xmm6
	movdqa		%xmm6, %xmm0
	movdqa		%xmm0, %xmm2
	movl		-8(%rbp), %eax
	movl		%eax, -88(%rbp)
	movd		-88(%rbp), %xmm6
	pshufd		$0, %xmm6, %xmm0
	movdqa		%xmm0, %xmm3
	pmuludq		-32(%rbp), %xmm3
	movdqa		%xmm0, %xmm1
	psrlq		$32, %xmm1
	movdqa		-32(%rbp), %xmm0
	psrlq		$32, %xmm0
	pmuludq		%xmm0, %xmm1
	pshufd		$8, %xmm3, %xmm0
	pshufd		$8, %xmm1, %xmm1
	punpckldq	%xmm1, %xmm0
	movdqa		%xmm0, %xmm1
	movl		$255, %eax
	subl		-4(%rbp), %eax
	movl		%eax, -88(%rbp)
	movd		-88(%rbp), %xmm7
	pshufd		$0, %xmm7, %xmm0
	movdqa		%xmm1, %xmm3
	pmuludq		%xmm0, %xmm3
	psrlq		$32, %xmm1
	psrlq		$32, %xmm0
	pmuludq		%xmm0, %xmm1
	pshufd		$8, %xmm3, %xmm0
	pshufd		$8, %xmm1, %xmm1
	punpckldq	%xmm1, %xmm0
	movd		%xmm0, %ecx
	movl		$-2130607613, %edx
	movl		%ecx, %eax
	imull		%edx
	leal		(%rdx,%rcx), %eax
	sarl		$15, %eax
	movl		%eax, %edx
	movl		%ecx, %eax
	sarl		$31, %eax
	subl		%eax, %edx
	movl		%edx, -88(%rbp)
	pshufd		$85, %xmm0, %xmm1
	movd		%xmm1, %ecx
	movl		$-2130607613, %edx
	movl		%ecx, %eax
	imull		%edx
	leal		(%rdx,%rcx), %eax
	sarl		$15, %eax
	movl		%eax, %edx
	movl		%ecx, %eax
	sarl		$31, %eax
	subl		%eax, %edx
	movl		%edx, -92(%rbp)
	movdqa		%xmm0, %xmm1
	punpckhdq	%xmm0, %xmm1
	movd		%xmm1, %ecx
	movl		$-2130607613, %edx
	movl		%ecx, %eax
	imull		%edx
	leal		(%rdx,%rcx), %eax
	sarl		$15, %eax
	movl		%eax, %edx
	movl		%ecx, %eax
	sarl		$31, %eax
	subl		%eax, %edx
	movl		%edx, -96(%rbp)
	pshufd		$255, %xmm0, %xmm0
	movd		%xmm0, %ecx
	movl		$-2130607613, %edx
	movl		%ecx, %eax
	imull		%edx
	leal		(%rdx,%rcx), %eax
	sarl		$15, %eax
	movl		%eax, %edx
	movl		%ecx, %eax
	sarl		$31, %eax
	subl		%eax, %edx
	movl		%edx, -100(%rbp)
	movd		-96(%rbp), %xmm0
	movd		-100(%rbp), %xmm4
	punpckldq	%xmm4, %xmm0
	movd		-88(%rbp), %xmm1
	movd		-92(%rbp), %xmm5
	punpckldq	%xmm5, %xmm1
	movdqa		%xmm1, %xmm7
	punpcklqdq	%xmm0, %xmm7
	movdqa		%xmm7, %xmm0
	paddd		%xmm0, %xmm2
	movdqa		%xmm2, %xmm1
	movdqa		%xmm1, %xmm0
	pslld		$8, %xmm0
	psubd		%xmm1, %xmm0
	movdqa		%xmm0, %xmm1
	movl		-12(%rbp), %eax
	movl		%eax, -88(%rbp)
	movd		-88(%rbp), %xmm6
	pshufd		$0, %xmm6, %xmm0
	movd		%xmm1, %eax
	movd		%xmm0, %esi
	cltd
	idivl		%esi
	movl		%eax, -88(%rbp)
	pshufd		$85, %xmm1, %xmm2
	movd		%xmm2, %eax
	pshufd		$85, %xmm0, %xmm2
	movd		%xmm2, %edi
	cltd
	idivl		%edi
	movl		%eax, -92(%rbp)
	movdqa		%xmm1, %xmm2
	punpckhdq	%xmm1, %xmm2
	movd		%xmm2, %eax
	movdqa		%xmm0, %xmm2
	punpckhdq	%xmm0, %xmm2
	movd		%xmm2, %esi
	cltd
	idivl		%esi
	movl		%eax, -96(%rbp)
	pshufd		$255, %xmm1, %xmm1
	movd		%xmm1, %eax
	pshufd		$255, %xmm0, %xmm0
	movd		%xmm0, %edi
	cltd
	idivl		%edi
	movl		%eax, -100(%rbp)
	movd		-96(%rbp), %xmm0
	movd		-100(%rbp), %xmm7
	punpckldq	%xmm7, %xmm0
	movd		-88(%rbp), %xmm1
	movd		-92(%rbp), %xmm4
	punpckldq	%xmm4, %xmm1
	movdqa		%xmm1, %xmm4
	punpcklqdq	%xmm0, %xmm4
	movdqa		%xmm4, %xmm0
	movaps		%xmm0, -64(%rbp)
	
	movl		-64(%rbp), %edx
	movb		%dl, (%rsi)
	movl		-60(%rbp), %edx
	mov		%dl, 1(%rsi)
	movl		-56(%rbp), %edx
	mov		%dl, 2(%rsi)
	movl		-52(%rbp), %edx
	mov		%dl, 3(%rsi)
	
	movl		%r8d, %edx
	movslq		%edx, %rdx
	addq		%rsi, %rdx
	movl		-12(%rbp), %eax
	movb		%al, (%rdx)
.L113:
	popq		%rbp
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE14:
	.size		ddiBlendPixel, .-ddiBlendPixel
