	.text
	.global cpu_fresh_stack
cpu_fresh_stack:
	/* Move params to registers. */
	mov	+4(%esp), %ecx		// Function to call
	mov	+8(%esp), %edx		// New stack

	/* Switch to fresh stack. */
	mov	%edx,	%esp		// New stack
	mov	$0,	%ebp		// Null stack frame

	/* Save function to new stack. */
	push	%ecx

1:	/* Call function. */
	mov	+0(%esp), %ecx		// Get function from new frame
	call	*%ecx			// Call function

	/* If function returns, simply call it again. */
	jmp	1b

	.text
	.global	cpu_task_save
	.global	cpu_task_restore
	.type	cpu_task_save, @function
	.type	cpu_task_restore, @function

cpu_task_save:
	mov	+4(%esp), %ecx		// arg1: struct cpu_task_save ptr
	/* Fetch the return adress early */
	mov	+0(%esp), %edx

	/* Save state into cpu_task_save struct. */

	/* Save registers to struct */
	mov %esp, 0(%ecx)
	mov %ebp, 4(%ecx)
	mov %ebx, 8(%ecx)
	mov %esi, 12(%ecx)
	mov %edi, 16(%ecx)

	/* Save a return adress */
	mov %edx, 20(%ecx)

	/* Return to caller with return value 0. */
	mov	$0, %eax		// 0 return code to indicate save
	ret

cpu_task_restore:
	mov	+4(%esp), %ecx		// arg1: struct cpu_task_save ptr
	mov	+8(%esp), %eax		// arg2: return code

	/* Restore state from cpu_task_save struct.
	 * Should basically be the reverse of cpu_task_save. */

	/* Restore registers */
    mov 0(%ecx), %esp
    mov 4(%ecx), %ebp
    mov 8(%ecx), %ebx
    mov 12(%ecx), %esi
    mov 16(%ecx), %edi

	/* Restore the return adress */
	mov 20(%ecx), %edx		
	/* Allocate space for a return adress on the stack */
    sub $4, %esp			
	/* Add the return adress to the stack */
    mov %edx, (%esp)	

	/* Return to caller of cpu_task_save with given return value. */
	ret

