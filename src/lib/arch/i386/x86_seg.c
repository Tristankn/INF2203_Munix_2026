#include "x86_seg.h"

#include "abi.h"
#include "cpu.h"
#include "cpu_interrupt.h"

#include <drivers/log.h>

#include <core/compiler.h>
#include <core/inttypes.h>
#include <core/macros.h>
#include <core/sprintf.h>

#include <stdnoreturn.h>

/** Segment descriptor */
struct ATTR_PACKED ATTR_ALIGNED(8) segdesc32 {
    unsigned limit_low   : 16;
    unsigned base_low    : 16;
    unsigned base_mid    : 8;
    unsigned type        : 5;
    unsigned dpl         : 2;
    unsigned present     : 1;
    unsigned limit_high  : 4;
    unsigned avail       : 1; ///< Available for software use
    unsigned             : 1; // Reserved
    unsigned db          : 1;
    unsigned granularity : 1;
    unsigned base_high   : 8;
};

/** Task State Segment */
struct ATTR_PACKED tss32 {
    uint32_t backlink;
    uint32_t esp0; // Stack for privilege level 0
    uint16_t ss0;
    uint16_t _reserved0;
    uint32_t esp1; // Stack for privilege level 1
    uint16_t ss1;
    uint16_t _reserved1;
    uint32_t esp2; // Stack for privilege level 2
    uint16_t ss2;
    uint16_t _reserved2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint16_t es;
    uint16_t _reserved3;
    uint16_t cs;
    uint16_t _reserved4;
    uint16_t ss;
    uint16_t _reserved5;
    uint16_t ds;
    uint16_t _reserved6;
    uint16_t fs;
    uint16_t _reserved7;
    uint16_t gs;
    uint16_t _reserved8;
    uint16_t ldt_selector;
    uint16_t _reserved9;
    uint16_t debug_trap;
    uint16_t iomap_base;
};

/* === Inline Assembly for CPU operations === */

/** LGDT: Load Global Descriptor Table */
static inline void x86_lgdt(const struct x86_pseudodesc32 *desc)
{
    asm volatile("lgdt %0" ::"m"(*desc));
}

/** SGDT: Store Global Descriptor Table */
static inline void x86_sgdt(struct x86_pseudodesc32 *desc)
{
    asm volatile("sgdt %0" : "=m"(*desc) :);
}

/** LTR: Load Task Register */
static inline void ltr(uint16_t segsel)
{
    asm volatile("ltr %0" ::"mr"(segsel));
}

/* === To-string functions for debugging === */

/** Decode a Segment Descriptor's type field (@ref segdesc32.type) */
int x86st_tostr(char *buf, size_t n, unsigned segtype)
{
    unsigned s = segtype & 0x10; // Bit 5 is S bit (System), 0 = System Seg
    if (!s) {
        /* System segment types */
        const char *str;
        switch (segtype) {
        case 0x2: str = "LDT"; break;
        case 0x5: str = "Task Gate"; break;
        case 0x9: str = "TSS (avail)"; break;
        case 0xb: str = "TSS (busy)"; break;
        case 0xc: str = "Call Gate"; break;
        case 0xe: str = "Interrupt Gate"; break;
        case 0xf: str = "Trap Gate"; break;
        default: str = "other"; break;
        }
        return snprintf(buf, n, "%s", str);
    } else {
        /* Code or data segment */
        unsigned iscode = segtype & (1 << 4);
        char     tflags[4];
        flagstr(tflags, segtype, 3, iscode ? "CRA" : "EWA", NULL);
        return snprintf(buf, n, "%s:%s", iscode ? "code" : "data", tflags);
    }
}

/** Decode a Segment Descriptor */
static int segdesc32_tostr(char *buf, size_t n, const struct segdesc32 *seg)
{
    char *pos = buf, *end = buf + n;

    uintptr_t base =
            (seg->base_high << 24) | (seg->base_mid << 16) | seg->base_low;
    size_t limit = (seg->limit_high << 16) | seg->limit_low;
    if (seg->granularity) limit = (limit << 12) | 0xfff;

    pos += snprintf(
            pos, BUFREM(pos, end), "0x%.8" PRIxPTR ":%.8zx", base, limit
    );

    uint32_t high32 = *((uint32_t *) seg + 1);
    char     flags[5];
    flagstr(flags, high32 >> 20, 4, "GDAL", NULL);
    char P = high32 & (1 << 15) ? 'P' : '-';
    pos += snprintf(pos, BUFREM(pos, end), " %s %c", flags, P);

    pos += snprintf(pos, BUFREM(pos, end), " DPL%u ", seg->dpl);
    pos += x86st_tostr(pos, BUFREM(pos, end), seg->type);
    return pos - buf;
}

/** Decode a segment selector */
int x86_segsel_tostr(char *buf, size_t n, x86_segsel_t segsel)
{
    size_t      idx = segsel >> 3;
    const char *tbl = segsel & 0x4 ? "LDT" : "GDT";
    cpupl_t     rpl = segsel & 0x3;
    return snprintf(buf, n, "%s[%zu] RPL%u", tbl, idx, rpl);
}

/** Read and decode all segment registers */
static int x86_segregs_tostr(char *buf, size_t n)
{
    char        *pos = buf, *end = buf + n;
    x86_segsel_t cs, ds, es, fs, gs, ss;

#define printreg(REG) \
    x86_get_reg(#REG, REG); \
    pos += snprintf(pos, BUFREM(pos, end), "\n\t%s = %#4hx (", #REG, REG); \
    pos += x86_segsel_tostr(pos, BUFREM(pos, end), REG); \
    pos += snprintf(pos, BUFREM(pos, end), ")")

    printreg(cs);
    printreg(ds);
    printreg(es);
    printreg(fs);
    printreg(gs);
    printreg(ss);

    ureg_t bp, sp;
    x86_get_reg("ebp", bp);
    x86_get_reg("esp", sp);
    pos += snprintf(pos, BUFREM(pos, end), "\n\t%s = " FMT_REG, "bp", bp);
    pos += snprintf(pos, BUFREM(pos, end), "\n\t%s = " FMT_REG, "sp", sp);

    return pos - buf;
}

/** Decode a Pseudo-Descriptor */
static int
segdesc32_table_tostr(char *buf, size_t n, const struct x86_pseudodesc32 *desc)
{
    char *pos = buf, *end = buf + n;
    pos += snprintf(pos, BUFREM(pos, end), "%p ", desc->base);
    pos += snprintf(pos, BUFREM(pos, end), "sz:%u", desc->limit);

    if (!desc->base) {
        pos += snprintf(pos, BUFREM(pos, end), " (not initialized?)");
        return pos - buf;
    }

    size_t segct = desc->limit / sizeof(struct segdesc32);
    for (size_t i = 0; i < segct; i++) {
        uint64_t          rawseg = *((uint64_t *) desc->base + i);
        struct segdesc32 *seg    = (struct segdesc32 *) desc->base + i;
        pos += snprintf(
                pos, BUFREM(pos, end), "\n\t[%zu] = 0x%.16" PRIx64 " = ", i,
                rawseg
        );
        pos += segdesc32_tostr(pos, BUFREM(pos, end), seg);
    }

    return pos - buf;
}

/**
 * Common field settings used for GDT Segment Descriptors
 *
 * - The Present flag is set
 * - The D/B flag is set for 32-bit code (clear is for 16-bit code)
 * - Base and Limit are set so that the segment covers the entire 32-bit
 *   address space:
 *      - Base = 0
 *      - Limit = 0xffff with Granularity bit set
 */
#define GDT_COMMON \
    .limit_low = 0xffff, .limit_high = 0xf, .granularity = 1, .present = 1, \
    .db = 1

/** Our kernel's Global Descriptor Table */
static struct segdesc32 kernel_gdt[] = {
        [KSEG_NULL] = {}, // Required null descriptor
        [KSEG_KERNEL_CODE] =
                {GDT_COMMON, .type = X86ST_CODE_R, .dpl = PL_KERNEL},
        [KSEG_KERNEL_DATA] =
                {GDT_COMMON, .type = X86ST_DATA_W, .dpl = PL_KERNEL},
        [KSEG_USER_CODE] = {GDT_COMMON, .type = X86ST_CODE_R, .dpl = PL_USER},
        [KSEG_USER_DATA] = {GDT_COMMON, .type = X86ST_DATA_W, .dpl = PL_USER},
        [KSEG_TSS]       = {}, // Will be initialized at runtime
};

/** A dedicated Pseudo-Descriptor to point to the GDT */
static const struct x86_pseudodesc32 KERNEL_GDTPD = {
        .base = kernel_gdt, .limit = sizeof(kernel_gdt)};

/** A Single Task State Segment used to set kernel stack location */
static struct tss32 kernel_tss;

/* === Init functions === */

static int init_gdt(void)
{
    const size_t DBGSZ = 1024;
    char         dbgbuf[DBGSZ];

    /* Check existing GDT. */
    struct x86_pseudodesc32 gdt_pd;
    x86_sgdt(&gdt_pd);
    pr_debug(
            "current GDT: %s\n",
            (segdesc32_table_tostr(dbgbuf, DBGSZ, &gdt_pd), dbgbuf)
    );
    pr_debug(
            "segment registers: %s\n",
            (x86_segregs_tostr(dbgbuf, DBGSZ), dbgbuf)
    );

    /* Install our GDT. */
    pr_debug("installing new GDT %p ...\n", KERNEL_GDTPD.base);
    x86_lgdt(&KERNEL_GDTPD);

    /* Check GDT. */
    x86_sgdt(&gdt_pd);
    pr_info("installed  new GDT %s\n",
            (segdesc32_table_tostr(dbgbuf, DBGSZ, &gdt_pd), dbgbuf));

    /* Set code and data segments. */
    const x86_segsel_t SEL_KCODE =
            X86_SEGSEL_INIT(KSEG_KERNEL_CODE, PL_KERNEL);
    const x86_segsel_t SEL_KDATA =
            X86_SEGSEL_INIT(KSEG_KERNEL_DATA, PL_KERNEL);

    pr_debug("setting CS=%#.2x, data degs=%#.2x ...\n", SEL_KCODE, SEL_KDATA);
    x86_set_reg("ss", SEL_KDATA);
    x86_set_reg("gs", SEL_KDATA);
    x86_set_reg("fs", SEL_KDATA);
    x86_set_reg("es", SEL_KDATA);
    x86_set_reg("ds", SEL_KDATA);
    x86_set_cs(SEL_KCODE);
    pr_debug(
            "updated segment registers: %s\n",
            (x86_segregs_tostr(dbgbuf, DBGSZ), dbgbuf)
    );

    return 0;
}

static int init_tss(void)
{
    const size_t DBGSZ = 1024;
    char         dbgbuf[DBGSZ];

    /* Set up TSS. */
    kernel_gdt[KSEG_TSS] = (struct segdesc32){
            .base_low    = ((uintptr_t) &kernel_tss & 0x0000ffff),
            .base_mid    = ((uintptr_t) &kernel_tss & 0x00ff0000) >> 16,
            .base_high   = ((uintptr_t) &kernel_tss & 0xff000000) >> 24,
            .limit_low   = (sizeof(struct tss32) & 0x0ffff),
            .limit_high  = (sizeof(struct tss32) & 0xf0000) >> 16,
            .granularity = 0,
            .present     = 1,
            .db          = 1,
            .type        = X86ST_TSS,
            .dpl         = PL_KERNEL,
    };
    pr_info("updated GDT with TSS: %s\n",
            (segdesc32_table_tostr(dbgbuf, DBGSZ, &KERNEL_GDTPD), dbgbuf));

    x86_segsel_t tss_selector = X86_SEGSEL_INIT(KSEG_TSS, PL_KERNEL);
    pr_debug(
            "loading task register w/selector %#.4x (%s) ...\n", tss_selector,
            (x86_segsel_tostr(dbgbuf, DBGSZ, tss_selector), dbgbuf)
    );
    ltr(tss_selector);
    pr_debug("loaded  task register\n");

    return 0;
}

int init_cpu(void)
{
    int res;

    res = init_gdt();
    log_result(res, "set up GDT\n");
    if (res < 0) return res;

    res = x86_init_idt();
    log_result(res, "set up IDT\n");
    if (res < 0) return res;

    res = init_tss();
    log_result(res, "set up TSS\n");
    if (res < 0) return res;

    return 0;
}

void cpu_user_kstack_set(uintptr_t kstack_addr)
{
    x86_segsel_t kdata_segsel = X86_SEGSEL_INIT(KSEG_KERNEL_DATA, PL_KERNEL);

    kernel_tss.ss0  = kdata_segsel;
    kernel_tss.esp0 = kstack_addr;
}

noreturn void cpu_user_start(uintptr_t start_addr, uintptr_t ustack_addr)
{
    x86_segsel_t codeseg, dataseg;
    codeseg = X86_SEGSEL_INIT(KSEG_USER_CODE, PL_USER);
    dataseg = X86_SEGSEL_INIT(KSEG_USER_DATA, PL_USER);

    ureg_t flags = 0;
    //flags |= (1 << 9); // Enable interrupts in user space.

    /* On i386, the easiest way to switch to a lower privilege level
     * is to return from an interrupt.
     * We will create a fake interrupt frame on the stack
     * and then return from it. */
    struct x86_intr_frame frame = {
            .ip    = start_addr,
            .cs    = codeseg,
            .flags = flags,
            .sp    = ustack_addr,
            .ss    = dataseg,
    };

    const size_t DBGSZ = 256;
    char         dbgbuf[DBGSZ];
    pr_trace(
            "launching process by returning from fake interrupt %p:%s\n",
            &frame, (x86_intr_frame_tostr(dbgbuf, DBGSZ, &frame), dbgbuf)
    );

    pr_debug(
            "jumping to user space: start_addr=%p, ustack=%p\n",
            (void *) start_addr, (void *) ustack_addr
    );

    /* Inline assembly to switch data segments
     * and then IRET to return from fake interrupt. */
    asm volatile(
            /* Set data segment registers. */
            "mov    %[udata], %%ds\n\t"
            "mov    %[udata], %%es\n\t"
            /* Set stack pointer and return from fake interrupt. */
            "mov    %[frame], %%esp\n\t"
            "iret" ::[frame] "r"(&frame),
            [udata] "r"(dataseg)
    );

    /* Unreachable part of function.
     * Control will never come back to this function after the inline IRET. */
    __builtin_unreachable();
}

