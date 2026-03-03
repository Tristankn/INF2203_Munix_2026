#include <errno.h>		// Get error codes
#if __linux__
#include <asm/unistd_64.h>	// Get Linux syscall numbers
#endif

	/*
	 * System V args:	%rdi %rsi %rdx %rcx %r8  %r9 s+8
	 * Linux syscall:	%rax %rdi %rsi %rdx %r10 %r8 %r9
	 */
	.text
	.global	syscall
	.type	syscall, @function
syscall:
	/* Shift registers from incoming call to syscall */
	mov	%rdi, %rax	// fn arg 1 -> syscall number
	mov	%rsi, %rdi	// fn arg 2 -> syscall arg 1
	mov	%rdx, %rsi	// fn arg 3 -> syscall arg 2
	mov	%rcx, %rdx	// fn arg 4 -> syscall arg 3
	mov	%r8,  %r10	// fn arg 5 -> syscall arg 4
	mov	%r9,  %r8	// fn arg 6 -> syscall arg 5
	mov	+8(%rsp), %r9	// fn arg 7 -> syscall arg 6
	syscall
	ret

#if __linux__
.macro	PUSH_TO	stack_reg, val
	sub	$8, \stack_reg
	mov	\val, (\stack_reg)
.endm

	/*
	 * C Params             C ABI           Syscall         Linux ABI
	 * ---------            ------          --------        ----------
	 * 1. fn                %rdi            -> n/a
	 * 2. stack             %rsi            -> arg2         %rsi
	 * 3. flags             %rdx            -> arg1         %rdi
	 * 4. arg               %rcx            -> n/a
	 * 5. parent_tid        %r8             -> arg3         %rdx
	 * 6. tls               %r9             -> arg5         %r8
	 * 7. child_tid/futex   +8(%rsp)        -> arg4         %r10
	 *
	 * - The thread function start address and its argument are not
	 *   passed to the raw system call. They are handled by this wrapper.
	 *
	 * For details, see:
	 * - Man pages clone(2), syscall(2)
	 * - Sys V ABI supplement for x86-64
	 */
	.text
	.global syscall_clone_linux
	.type	syscall_clone_linux, @function
syscall_clone_linux:
	/* Save important values on child stack. */
	and	$~(16-1), %rsi		// Align stack to 16-byte boundary.
	PUSH_TO	%rsi, %rcx		// Push fn arg.
	PUSH_TO	%rsi, %rdi		// Push fn ptr.

	/* Make Linux clone syscall. */
	mov	$__NR_clone,	%rax	// syscall number
	mov	%rdx,		%rdi	// flags
	mov	%r8,		%rdx	// p_tid
	mov	%r9,		%r8 	// TLS
	mov	+8(%rsp),	%r10	// c_tid/futex
	syscall
	test	%rax, %rax		// Check return value.
	jz	1f			// If ret val == 0, go to child thread.
	ret				// Else return.

1:	/* Child thread return path */
	mov	$0, %rbp		// Clearn frame pointer

	/* Child thread: Call thread function. */
	pop	%rax			// Pop fn ptr
	pop	%rdi			// Pop fn arg	-> arg1
	call	*%rax			// Call fn ptr.

	/* Child thread: Call exit. */
	mov	%rax, %rdi		// Use fn ret val as arg to exit.
	mov	$__NR_exit, %rax	// Syscall number
	syscall

	/* No return */
	hlt
#endif /* __linux__ */
