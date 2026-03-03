#ifndef CPU_X86_H
#define CPU_X86_H

#include <core/compiler.h>
#include <core/inttypes.h>

#include <stddef.h>
#include <stdint.h>

#if __i386__
typedef uint32_t ureg_t; ///< Int type for general-purpose register (32-bit)
#elif __x86_64__
typedef uint64_t ureg_t; ///< Int type for general-purpose register (64-bit)
#endif

typedef uint16_t ioport_t; ///< I/O port number

typedef unsigned char cpupl_t; ///< CPU Privilege Level

#define PRIdREG "zd"     ///< ureg format snippet: signed decimal
#define PRIuREG "zu"     ///< ureg format snippet: unsigned decimal
#define PRIxREG "zx"     ///< ureg format snippet: hex
#define FMT_REG "%#10zx" ///< ureg convenient format

#define PRIdPORT "hd"    ///< ioport format snippet: signed decimal
#define PRIuPORT "hu"    ///< ioport format snippet: unsigned decimal
#define PRIxPORT "hx"    ///< ioport format snippet: hex
#define FMT_PORT "%#4hx" ///< ioport convenient printf format

#define PRIdPL "hhd"  ///< priv level format snippet: signed decimal
#define PRIuPL "hhu"  ///< priv level format snippet: unsigned decimal
#define PRIxPL "hhx"  ///< priv level format snippet: hex
#define FMT_PL "%hhu" ///< priv level convenient printf format

#define PL_KERNEL 0 ///< priv level for kernel
#define PL_USER   3 ///< priv level for user

/** Read input from an I/O port */
#define IO_IN(VAL, PORT) \
    asm inline volatile( \
            "in	%[port],	%[val]\n" \
            : [val] "=a"(VAL)   /* A register */ \
            : [port] "Nd"(PORT) /* D register or immediate byte */ \
    )

/** Send output to an I/O port */
#define IO_OUT(VAL, PORT) \
    asm inline volatile( \
            "out	%[val],	%[port]\n" \
            : \
            : [val] "a"(VAL),   /* A register */ \
              [port] "Nd"(PORT) /* D register or immediate byte */ \
    )

static inline uint8_t inb(ioport_t port)
{
    uint8_t ret;
    IO_IN(ret, port);
    return ret;
}

static inline uint16_t inw(ioport_t port)
{
    uint16_t ret;
    IO_IN(ret, port);
    return ret;
}

static inline uint32_t inl(ioport_t port)
{
    uint32_t ret;
    IO_IN(ret, port);
    return ret;
}

static inline void outb(uint8_t val, ioport_t port) { IO_OUT(val, port); }
static inline void outw(uint16_t val, ioport_t port) { IO_OUT(val, port); }
static inline void outl(uint32_t val, ioport_t port) { IO_OUT(val, port); }

static inline void cpu_halt(void) { asm inline volatile("hlt"); }

int init_cpu(void);

void           cpu_user_kstack_set(uintptr_t kstack_addr);
_Noreturn void cpu_user_start(uintptr_t start_addr, uintptr_t ustack_addr);

long syscall_dispatch(
        long   number,
        ureg_t arg1,
        ureg_t arg2,
        ureg_t arg3,
        ureg_t arg4,
        ureg_t arg5
);

_Noreturn void cpu_fresh_stack(void (*fn)(void), uintptr_t kstack);

struct cpu_task_save {
    /* TODO: Decide what state to save in struct */
};

ATTR_RETURNS_TWICE int cpu_task_save(struct cpu_task_save *save_state);
_Noreturn int cpu_task_restore(struct cpu_task_save *save_state, int status);

typedef uint64_t cputick_t;

/** Read the CPU time stamp counter (CPU clock ticks) */
static inline cputick_t cpu_ticks(void)
{
    cputick_t tsc;

    /* RDTSC reads a 64-bit timestamp counter into EDX:EAX (high bits in EDX,
     * low bits in EAX).
     *
     * The "A" operand constraint is specifically for this register
     * configuration on x86-32.
     *
     * Note that if/when we move to x86-64, this will need to be changed.
     * See <https://gcc.gnu.org/onlinedocs/gcc/Machine-Constraints.html> */
    asm inline volatile("rdtsc" : "=A"(tsc));

    return tsc;
}

#endif /* CPU_X86_H */
