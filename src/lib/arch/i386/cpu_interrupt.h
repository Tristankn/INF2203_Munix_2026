#ifndef CPU_X86_INTERRUPT_H
#define CPU_X86_INTERRUPT_H

#include "cpu.h"

#include <core/compiler.h>

/** @name Interrupt Vector Type */
///@{
typedef unsigned char ivec_t; ///< CPU Interrupt Vector
#define PRIdIVEC "hhd"        ///< printf format snippet: signed decimal
#define PRIuIVEC "hhu"        ///< printf format snippet: unsigned decimal
#define PRIxIVEC "hhx"        ///< printf format snippet: hex
#define FMT_IVEC "%hhu"       ///< convenient printf format
///@}

/**
 * Interrupt Vectors
 *
 * Here we only include vectors that we are likely to handle specifically.
 * See the CPU manual for a full list.
 */
enum ivec {
    IVEC_PF = 14, ///< \#PF - Page Fault
    IVEC_USER_START = 32, ///< Start of vectors available to the OS
    IVEC_IRQ_0 = 32, ///< Start of IRQ vectors
    IVEC_SYSCALL = 48, ///< Interrupt vector for syscalls
};

/** Values placed onto the stack by an x86 interrupt. */
struct ATTR_PACKED x86_intr_frame {
    ureg_t ip;    ///< Instruction pointer
    ureg_t cs;    ///< Code Segment selector
    ureg_t flags; ///< Flags register
    ureg_t sp;    ///< Stack Pointer
    ureg_t ss;    ///< Stack Segment selector
};

/** Interrupt context data to send to @ref interrupt_dispatch */
struct intrdata {
    ureg_t                 errcode;    ///< Error code
    struct x86_intr_frame *frame;      ///< Interrupt stack frame
    uintptr_t              fault_addr; ///< For page faults, fault address
};

const char *ivec_name(ivec_t ivec);
int x86_intr_frame_tostr(char *buf, size_t n, struct x86_intr_frame *iframe);
int intrdata_tostr(char *buf, size_t n, ivec_t ivec, struct intrdata *idata);

/** Check if interupts are enabled. */
static inline int intr_isenabled(void)
{
    ureg_t flags;
    asm inline volatile(
            "pushf\n\t"     // Push flags register.
            "pop	%0" // Retrieve value and restore stack.
            : "=r"(flags)
    );
    return flags & (1 << 9); // Bit 9 is the interrupt flag (IF).
}

/** Set interrupt enabled status. */
static inline void intr_setenabled(int enabled)
{
    if (enabled) {
        asm inline volatile("sti");
    } else {
        asm inline volatile("cli");
    }
}

/** Is this interrupt a CPU-defined exception type? */
static inline int ivec_isexception(ivec_t ivec)
{
    return ivec < IVEC_USER_START;
}

/** Kernel function called on interrupt */
void interrupt_dispatch(ivec_t vec, struct intrdata *idata);

int x86_init_idt(void);

#endif /* CPU_X86_INTERRUPT_H */
