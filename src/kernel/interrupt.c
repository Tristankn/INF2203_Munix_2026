// #define LOG_LEVEL LOG_DEBUG

#include "kernel.h"
#include "process.h"

#include <cpu_interrupt.h>

#include <drivers/chip/intctl_8259.h>
#include <drivers/chip/pit_8253.h>
#include <drivers/log.h>

#include <core/errno.h>
#include <core/sprintf.h>

static void handle_exception(ivec_t ivec, struct intrdata *idata)
{
    const int dbgsz = 256;
    char      dbgbuf[dbgsz];

    pr_error(
            "process %d (%s): unhandled exception " FMT_IVEC " (%s) %s\n",
            current_process ? current_process->pid : 0,
            current_process ? current_process->name : "", ivec,
            ivec_name(ivec),
            (intrdata_tostr(dbgbuf, dbgsz, ivec, idata), dbgbuf)
    );

    /* Kill current process. */
    process_kill(current_process);
    kernel_noreturn();
}

/** Tally of IRQ counts */
static unsigned long long irq_cts[IRQ_MAX];

static int irq_cts_tostr(char *buf, size_t n)
{
    char *pos = buf, *end = buf + n;
    for (ivec_t i = 0; i < IRQ_MAX; i++)
        pos += snprintf(
                pos, BUFREM(pos, end), "%s%llu", i ? ", " : "", irq_cts[i]
        );
    return pos - buf;
}

static void handle_irq(ivec_t ivec, struct intrdata *idata)
{
    UNUSED(idata);
    const int dbgsz = 256;
    char      dbgbuf[dbgsz];

    ivec_t irq = ivec - IVEC_IRQ_0;
    irq_cts[irq]++;

    /* Handle IRQ. */
    switch (irq) {
    case IRQ_TIMER:

    default: {
        logf_once(
                LOG_INFO, "unhandled IRQ " FMT_IVEC " (%s)\n", irq,
                ivec_name(ivec)
        );
        pr_debug("IRQ cts: %s\n", (irq_cts_tostr(dbgbuf, dbgsz), dbgbuf));
        pic_send_eoi(irq);
        break;
    }
    }
}

void interrupt_dispatch(ivec_t ivec, struct intrdata *idata)
{
    pr_debug("interrupt %d (%s)\n", ivec, ivec_name(ivec));

    if (ivec_isexception(ivec)) return handle_exception(ivec, idata);

    if (IVEC_IRQ_0 <= ivec && ivec < IVEC_IRQ_0 + IRQ_MAX)
        return handle_irq(ivec, idata);
}

/* === Interrupt Controller Init === */

int init_int_controller(void)
{
    irqmask_t IRQS_TO_ENABLE = 0;
    //IRQS_TO_ENABLE |= (1 << IRQ_TIMER);

    /* Initialize Interrupt Controller. */
    pic_init(IVEC_IRQ_0);
    pic_set_mask(~IRQS_TO_ENABLE); // PIC mask is set-to-block, clear-to-enable

#if OM_IRQ_SET_LEVEL_MODE
    /* Set level mode interrupt detection for PCI lines ICH spec */
    outb(0x28, 0x4d0);
    outb(0x0e, 0x4d1);
#endif

    return 0;
}

/* === Timer IRQ, used for task preemption === */

int init_timer_interrupt(void)
{

    /* Target frequency for the task preempt interrupt, in Hz
     * Determines the length of thread time slices. */
    const unsigned int target_hz = 1000;

    /* Programmable Interval Timer mode used for task preempt interrupt
     *
     * The wiki at OSDev.org notes that operating systems typically use the
     * square wave mode (mode 3) to generate timer interrupts. However, we have
     * found that square wave mode seems to cause many spurious interrpts,
     * while rate generator mode (mode 2) does not, at least when running under
     * Bochs 2.7.
     *
     * See:
     * - OSDev wiki: <https://wiki.osdev.org/Programmable_Interval_Timer> */
    const enum pit_mode pit_mode = PIT_MODE_SQUARE_WAVE;

    pit_set_irq_freq(pit_mode, target_hz);
    return 0;
}

