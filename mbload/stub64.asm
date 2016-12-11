bits 64
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	xor rax, rax
	mov ss, ax
	
	mov rsp, [rsp+4]
	mov rax, [rsp+0x10]
	lgdt [rax]
	
	xor rbp, rbp
	mov rdi, rsp
	mov rax, [rsp+8]
	and rsp, ~0xF
	call rax
	
_hang2:
	cli
	hlt
	jmp _hang2
