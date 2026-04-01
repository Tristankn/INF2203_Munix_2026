#include "cpu_interrupt.h"

#include "x86_seg.h"

#include <drivers/chip/intctl_8259.h>
#include <drivers/log.h>

#include <core/compiler.h>
#include <core/inttypes.h>
#include <core/macros.h>
#include <core/sprintf.h>

/* === Interrupt Handlers and Interrupt Characteristics === */

/**
 * Common ISR function that is called by macro-generated ISRs below
 */
ATTR_CALLED_FROM_ISR
static void
isr_common_x86(ivec_t vec, struct x86_intr_frame *frame, ureg_t errcode)
{
    /* Complete transition to kernel mode by setting data segments. */
    x86_segsel_t ds, es;
    x86_get_reg("ds", ds);
    x86_get_reg("es", es);
    x86_set_reg("es", KSEG_KERNEL_DATA);
    x86_set_reg("ds", KSEG_KERNEL_DATA);

    /* Gather interrupt data. */
    struct intrdata idata = {.errcode = errcode, .frame = frame};
    if (vec == 14) x86_get_reg("cr2", idata.fault_addr);

    /* Call to kernel to actually handle interrupt. */
    interrupt_dispatch(vec, &idata);

    /* Restore previous data segments. */
    x86_set_reg("es", es);
    x86_set_reg("ds", ds);
}

/**
 * Second stage of syscall entry via interrupt
 *
 * 1. The assembly stage translates parameters.
 * 2. This C stage sets segment registers and calls dispatch function.
 */
ATTR_CALLED_FROM_ISR
long isr_syscall_stage2(
        long   number,
        ureg_t arg1,
        ureg_t arg2,
        ureg_t arg3,
        ureg_t arg4,
        ureg_t arg5
)
{
    /* Complete transition to kernel mode by setting data segments. */
    x86_segsel_t ds, es;
    x86_get_reg("ds", ds);
    x86_get_reg("es", es);
    x86_set_reg("es", KSEG_KERNEL_DATA);
    x86_set_reg("ds", KSEG_KERNEL_DATA);

    long res = syscall_dispatch(number, arg1, arg2, arg3, arg4, arg5);

    /* Restore previous data segments. */
    x86_set_reg("es", es);
    x86_set_reg("ds", ds);

    return res;
}

/** Declare an interrupt handler for the given vector */
#define ISR(VEC, FNNAME) \
    INTERRUPT_HANDLER \
    static void FNNAME(struct x86_intr_frame *frame) \
    { \
        isr_common_x86(VEC, frame, 0); \
    }

/** Declare an interrupt handler, with error code, for the given vector */
#define ISR_E(VEC, FNNAME) \
    INTERRUPT_HANDLER \
    static void FNNAME(struct x86_intr_frame *frame, ureg_t errcode) \
    { \
        isr_common_x86(VEC, frame, errcode); \
    }

struct handler_to_install {
    ivec_t ivec;
    void  *handler;
};

/** Get a string name for an interrupt/exception */
const char *ivec_name(ivec_t ivec)
{
    switch (ivec) {
    case 0: return "#DE - Divide Error";
    case 1: return "#DB - Debug Exception";
    case 2: return "NMI - Non-Maskable Interrupt";
    case 3: return "#BP - Breakpoint";
    case 4: return "#OF - Overflow";
    case 5: return "#BR - BOUND Range Exceeded";
    case 6: return "#UD - Undefined Opcode";
    case 7: return "#NM - No Math Coprocessor";
    case 8: return "#DF - Double Fault";
    case 9: return "Coprocessor Segment Overrun";
    case 10: return "#TS - Invalid TSS";
    case 11: return "#NP - Segment Not Present";
    case 12: return "#SS - Stack-Segment Fault";
    case 13: return "#GP - General Protection Fault";
    case 14: return "#PF - Page Fault";
    case 16: return "#MF - FPU Math Fault";
    case 17: return "#AC - Alignment Check";
    case 18: return "#MC - Machine Check";
    case 19: return "#XM - SIMD Exception";
    case 20: return "#VE - Virtualization Exception";
    case 21: return "#CP - Control Protection Exception";
    case IVEC_IRQ_0 + IRQ_TIMER: return "Timer IRQ";
    default: return "[unknown]";
    }
}

/**
 * Does this x86 exception push an error code onto the stack?
 */
#define ivec_haserrcode(IVEC) \
    (IVEC == 8 || (10 <= IVEC && IVEC <= 14) || IVEC == 17)

ISR(0, isr0);     ///< Handler for x86 \#DE Divide Error
ISR(6, isr6);     ///< Handler for x86 \#UD Undefined Opcode
ISR_E(8, isr8);   ///< Handler for x86 \#DF Double Fault
ISR_E(10, isr10); ///< Handler for x86 \#TS Invalid TSS
ISR_E(11, isr11); ///< Handler for x86 \#NP Segment Not Present
ISR_E(12, isr12); ///< Handler for x86 \#SS Stack-Segment Fault
ISR_E(13, isr13); ///< Handler for x86 \#GP General Protection Fault
ISR_E(14, isr14); ///< Handler for x86 \#PF Page Fault
ISR(IVEC_IRQ_0 + IRQ_TIMER, isr_irq0); ///< Handler for IRQ 0 (Timer)

/** Interrupt handler functions to install into the IDT. */
static const struct handler_to_install HANDLERS[] = {
        {0, isr0},                          /// Divide Error
        {6, isr6},                          /// UNdefined Opcode
        {8, isr8},                          /// Double Fault
        {10, isr10},                        /// Invalid TSS
        {11, isr11},                        /// Segment Not Present
        {12, isr12},                        /// Stack-Segment Fault
        {13, isr13},                        /// General Protection Fault
        {14, isr14},                        /// Page Fault
        {IVEC_IRQ_0 + IRQ_TIMER, isr_irq0}, /// IRQ 0: Timer
};

/* The kernel will provide a syscall entry interrupt handler. */
void isr_syscall(void);

/* === Interrupt Gate Descriptors and IDT === */

/** Gate descriptor */
struct ATTR_PACKED ATTR_ALIGNED(8) gate32 {
    unsigned offset_low  : 16;
    unsigned segsel      : 16;
    unsigned             : 8; // Reserved
    unsigned type        : 5;
    unsigned dpl         : 2;
    unsigned present     : 1;
    unsigned offset_high : 16;
};

#define INTR_GATE_MAX 64
static struct gate32 idt[INTR_GATE_MAX];

/** LIDT: Load Interrupt Descriptor Table Register (IDTR) */
static inline void lidt(struct x86_pseudodesc32 *desc)
{
    asm volatile("lidt %0" ::"m"(*desc));
}

/* === Printing Interrupt Information === */

static int gate32_tostr(char *buf, size_t n, struct gate32 *seg)
{
    char *pos = buf, *end = buf + n;

    uintptr_t base = (seg->offset_high << 16) | seg->offset_low;

    pos += snprintf(pos, BUFREM(pos, end), "%#" PRIxPTR, base);
    pos += snprintf(pos, BUFREM(pos, end), " CS=%#x (", seg->segsel);
    pos += x86_segsel_tostr(pos, BUFREM(pos, end), seg->segsel);
    char p = seg->present ? 'P' : '-';
    pos += snprintf(pos, BUFREM(pos, end), ") %c", p);
    pos += snprintf(pos, BUFREM(pos, end), " DPL%u ", seg->dpl);
    pos += x86st_tostr(pos, BUFREM(pos, end), seg->type);
    return pos - buf;
}

static int errcode_tostr(char *buf, size_t n, ivec_t ivec, ureg_t errcode)
{
    switch (ivec) {
    case 8:
    case 10:
    case 11:
    case 12:
    case 13: {
        /* Descriptor error: code is similar to a semgent selector. */
        unsigned idx = errcode >> 3;
        char     flags[4];
        flagstr(flags, errcode, 3, "LIE", "g-i");
        const char *tbl = errcode & 0x2   ? "IDT"
                          : errcode & 0x4 ? "LDT"
                                          : "GDT";
        return snprintf(buf, n, "%u|%s = %s[%u]", idx, flags, tbl, idx);
    }
    case 14: {
        char flags[9];
        flagstr(flags, errcode, 8, "HSKIVUWP", "---d-srp");
        char sgx = errcode & (1 << 15) ? 'X' : '-';
        return snprintf(buf, n, "%c|%s", sgx, flags);
    }
    case 17: {
        /* Alignment Check: code is null except perhaps EXT bit. */
        char ext = errcode & 0x1 ? 'E' : '-';
        return snprintf(buf, n, "#AC err %c", ext);
    }
    default: return snprintf(buf, n, "?"); // Other codes not supported.
    }
}

#define printval(LABEL, VAL) \
    snprintf(pos, BUFREM(pos, end), "\n\t%-11s = " FMT_REG, LABEL, VAL)

int x86_intr_frame_tostr(char *buf, size_t n, struct x86_intr_frame *iframe)
{
    char *pos = buf, *end = buf + n;

    pos += printval("ip", iframe->ip);
    pos += printval("cs", iframe->cs);
    pos += snprintf(pos, BUFREM(pos, end), " (");
    pos += x86_segsel_tostr(pos, BUFREM(pos, end), iframe->cs);
    pos += snprintf(pos, BUFREM(pos, end), ")");

    pos += printval("flags", iframe->flags);

    if (X86_SEGSEL_IDX(iframe->cs) != KSEG_KERNEL_CODE) {
        pos += printval("sp", iframe->sp);
        pos += printval("ss", iframe->ss);
        pos += snprintf(pos, BUFREM(pos, end), " (");
        pos += x86_segsel_tostr(pos, BUFREM(pos, end), iframe->ss);
        pos += snprintf(pos, BUFREM(pos, end), ")");
    } else {
        pos += snprintf(pos, BUFREM(pos, end), "%s", "\n\t(no stack switch)");
    }

    return pos - buf;
}

int intrdata_tostr(char *buf, size_t n, ivec_t ivec, struct intrdata *idata)
{
    char *pos = buf, *end = buf + n;

    /* Print error code, if present. */
    if (ivec_haserrcode(ivec)) {
        pos += printval("errcode", idata->errcode);
        pos += snprintf(pos, BUFREM(pos, end), " (");
        pos += errcode_tostr(pos, BUFREM(pos, end), ivec, idata->errcode);
        pos += snprintf(pos, BUFREM(pos, end), ")");
    } else {
        pos += snprintf(pos, BUFREM(pos, end), "%s", "\n\t(no error code)");
    }

    /* For page fault, also print fault address. */
    if (ivec == 14) pos += printval("fault_addr", idata->fault_addr);

    /* Print stack frame. */
    pos += x86_intr_frame_tostr(pos, BUFREM(pos, end), idata->frame);

    return pos - buf;
}

/* === IDT Setup === */

static int
x86_idt_add_isr(struct gate32 idt[], ivec_t vec, void *isr_fn, int dpl)
{
    unsigned gtype = X86ST_INTRGATE;

    idt[vec] = (struct gate32){
            .offset_low  = (uintptr_t) isr_fn & 0x0000ffff,
            .offset_high = ((uintptr_t) isr_fn & 0xffff0000) >> 16,
            .segsel      = X86_SEGSEL_INIT(KSEG_KERNEL_CODE, PL_KERNEL),
            .dpl         = dpl,
            .type        = gtype,
            .present     = 1,
    };

    const int DBGSZ = 256;
    char      dbgbuf[DBGSZ];
    pr_info("installed IDT[%2u] = %s\n", vec,
            (gate32_tostr(dbgbuf, DBGSZ, &idt[vec]), dbgbuf));
    return 0;
}

int x86_init_idt(void)
{
    struct x86_pseudodesc32 idt_pd = {.base = idt, .limit = sizeof(idt)};
    pr_info("installing new IDT %p ...\n", idt_pd.base);
    lidt(&idt_pd);
    pr_info("installed  new IDT %p\n", idt_pd.base);

    /* Install handlers. */
    for (size_t i = 0; i < ARRAY_SIZE(HANDLERS); i++)
        x86_idt_add_isr(idt, HANDLERS[i].ivec, HANDLERS[i].handler, PL_KERNEL);

    /* Add the syscall entry function with user-level DPL so that processes
     * can invoke it deliberately. */
    x86_idt_add_isr(idt, IVEC_SYSCALL, isr_syscall, PL_USER);

    return 0;
}

