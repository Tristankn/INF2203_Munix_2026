#include <errno.h>		// Get error codes
#if __linux__
#include <asm/unistd_64.h>	// Get Linux syscall numbers
#endif

#if __linux__
	.equ	IVEC_SYSCALL,		0x80
#elif __munix__
	.equ	IVEC_SYSCALL,		48
	/* Munix syscall numbers. Must match enum. */
	.equ	SYS_thrd_create,	3
	.equ	SYS_thrd_exit,		4
#endif

	/*
	 * System V args:	    f+8 +12 +16 +20 +24 +28 +32
	 * Linux syscall:	    eax ebx ecx edx esi edi ebp
	 */
	.text
	.global	syscall
	.type	syscall, @function
syscall:
	push	%ebp
	mov	%esp, %ebp

	/* Save nonvolatile registers. */
	push	%ebx
	push	%esi
	push	%edi

	mov	 +8(%ebp), %eax		// fn arg 1 -> syscall number
	mov	+12(%ebp), %ebx		// fn arg 2 -> syscall arg 1
	mov	+16(%ebp), %ecx		// fn arg 3 -> syscall arg 2
	mov	+20(%ebp), %edx		// fn arg 4 -> syscall arg 3
	mov	+24(%ebp), %esi		// fn arg 5 -> syscall arg 4
	mov	+28(%ebp), %edi		// fn arg 6 -> syscall arg 5
	push	%ebp
	mov	+32(%ebp), %ebp		// fn arg 7 -> syscall arg 6
	int	$IVEC_SYSCALL
	pop	%ebp

	/* Restore registers. */
	pop	%edi
	pop	%esi
	pop	%ebx

	pop	%ebp
	ret

.macro	PUSH_TO	stack_reg, val, scratch_reg=%eax
	sub	$4, \stack_reg
	mov	\val, \scratch_reg
	mov	\scratch_reg, (\stack_reg)
.endm

#if __munix__
	/*
	 * C Params             C ABI           Syscall         Linux ABI
	 * ---------            ------          --------        ----------
	 * 1. fn                +8(%ebp)        -> n/a
	 * 2. stack             +12(%ebp)       -> arg2         %ecx
	 * 3. arg               +16(%ebp)       -> n/a
	 */
	.text
	.global	syscall_thrd_create_munix
	.type	syscall_thrd_create_munix, @function
syscall_thrd_create_munix:
	push	%ebp
	mov	%esp, %ebp

	/* Save nonvolatile registers. */
	push	%ebx
	push	%esi
	push	%edi

	/* Save important values on child stack. */
	mov	+12(%ebp), %ecx		// Get child stack address.
	and	$~(4-1), %ecx		// Align to 4-byte boundary.
	PUSH_TO	%ecx, +16(%ebp)		// Push fn arg.
	PUSH_TO	%ecx,  +8(%ebp)		// Push fn ptr.

	/* Make Munix thrd_create syscall. */
	mov	$SYS_thrd_create,	%eax	// syscall number
	mov	$_thread_start_munix,	%ebx	// thread _start
	//mov	%ecx,			%ecx	// ustack (already in place)
	int	$IVEC_SYSCALL

	/* Restore registers. */
	pop	%edi
	pop	%esi
	pop	%ebx

	pop	%ebp
	ret

	/* Thread version of _start. */
_thread_start_munix:
	mov	$0, %ebp		// Clear frame pointer.

	/* New thread: Call thread function. */
	pop	%eax			// Pop fn ptr.
	call	*%eax			// Call function (arg is on stack)
	add	$4, %esp		// Pop fn arg

	/* New thread: Exit thread. */
	mov	%eax,	%ebx		// Use fn ret val as arg to exit
	mov	$SYS_thrd_exit, %eax	// Syscall number
	int	$IVEC_SYSCALL

	/* No return. */
	hlt
#endif /* __munix__ */

#if __linux__
	/*
	 * C Params             C ABI           Syscall         Linux ABI
	 * ---------            ------          --------        ----------
	 * 1. fn                +8(%ebp)        -> n/a
	 * 2. stack             +12(%ebp)       -> arg2         %ecx
	 * 3. flags             +16(%ebp)       -> arg1         %ebx
	 * 4. arg               +20(%ebp)       -> n/a
	 * 5. parent_tid        +24(%ebp)       -> arg3         %edx
	 * 6. tls               +28(%ebp)       -> arg4         %esi
	 * 7. child_tid/futex   +32(%ebp)       -> arg5         %edi
	 *
	 * - The thread function start address and its argument are not
	 *   passed to the raw system call. They are handled by this wrapper.
	 *
	 * For details, see:
	 * - Man pages clone(2), syscall(2)
	 * - Sys V ABI supplement for i386
	 */
	.text
	.global	syscall_clone_linux
	.type	syscall_clone_linux, @function
syscall_clone_linux:
	push	%ebp
	mov	%esp, %ebp

	/* Save nonvolatile registers. */
	push	%ebx
	push	%esi
	push	%edi

	/* Save important values on child stack. */
	mov	+12(%ebp), %ecx		// Get child stack address.
	and	$~(4-1), %ecx		// Align to 4-byte boundary.
	PUSH_TO	%ecx, +20(%ebp)		// Push fn arg.
	PUSH_TO	%ecx,  +8(%ebp)		// Push fn ptr.

	/* Make Linux clone syscall. */
	mov	$__NR_clone,	%eax	// syscall number
	mov	+16(%ebp),	%ebx	// flags
	mov	+24(%ebp),	%edx	// p_tid
	mov	+28(%ebp),	%esi	// TLS
	mov	+32(%ebp),	%edi	// c_tid/futex
	int	$IVEC_SYSCALL
	test	%eax, %eax		// Check return value.
	jz	1f			// If ret val == 0, go to child thread.

	/* Restore registers. */
	pop	%edi
	pop	%esi
	pop	%ebx

	pop	%ebp
	ret

1:	/* New thread return path */
	mov	$0, %ebp		// Clear frame pointer.

	/* New thread: Call thread function. */
	pop	%eax			// Pop fn ptr.
	call	*%eax			// Call function (arg is on stack).
	add	$4, %esp		// Pop fn arg.

	/* New thread: Exit thread. */
	mov	%eax,	%ebx		// Use fn ret val as arg to exit
	mov	$__NR_exit, %eax	// Syscall number
	int	$IVEC_SYSCALL

	/* No return. */
	hlt
#endif /* __linux__ */
