.text
.globl __release_and_exit

.type __release_and_exit, @function
__release_and_exit:
	// this function is called when a thread releases its own stack.
	// we must, without using the stack, release the spinlock passed
	// in %rdi, and then exit. the passed spinlock is the heap spinlock,
	// so the stack space will not be reallocated to another thread until
	// we release that lock.
	movb	$0,	(%rdi)
	mfence
	movq	$0,	%rdi
	ud2
	.hword 0
.size __release_and_exit, .-__release_and_exit
