#define LOG_LEVEL LOG_DEBUG

#include "pit_8253.h"

#include <cpu.h>

#include <drivers/log.h>

#include <stddef.h>

/* === PC Speaker === */

#undef pr_fmt
#define pr_fmt(fmt) "pc_spkr: " fmt

inline uint8_t pcspkr_read(void) { return inb(PCSPKR_PORT); }

uint8_t pcspkr_send_cmd(uint8_t flags)
{
    /* The PC speaker port 0x61 has other functionality crammed into its other
     * bits. Most references recommend reading from the port first and then
     * changing only the PC speaker bits. */
    uint8_t start_state = inb(PCSPKR_PORT);
    uint8_t new_state   = (start_state & ~PCSPKR_CMD_MASK) | flags;
    pr_debug(
            "updating state: from %#.2x to %#.2x (pos %d, mode %d)\n",
            start_state, new_state, (flags & 0x02) >> 1, (flags & 0x01)
    );
    outb(new_state, PCSPKR_PORT);
    return start_state;
}

void pcspkr_restore(uint8_t prev_state)
{
    pr_debug("restoring state: %#.2x\n", prev_state);
    outb(prev_state, PCSPKR_PORT);
}

/* === Programmable Interval Timer === */

#undef pr_fmt
#define pr_fmt(fmt) "pit_8253: " fmt

#define FMT_HZ  "%.4g Hz"
#define FMT_SEC "%.4g s"

void pit_send_cmd(enum pit_channel channel, enum pit_mode mode, uint16_t count)
{
    unsigned int cmd_byte =
            pit_cmd_byte(channel, PIT_RW_LSB_MSB, mode, PIT_BASE2);
    unsigned int port = PIT_PORT_CH0 + channel;
    pr_debug(
            "sending cmd %#.2x: ch %u, mode %u, ct %#.4x = %u = " FMT_SEC "\n",
            cmd_byte, channel, mode, count, count, (double) count / PIT_BASE_HZ
    );

    outb(cmd_byte, PIT_PORT_MODE_CMD);
    outb(count & 0xff, port);
    outb(count >> 8 & 0xff, port);
}

unsigned int pit_set_irq_freq(enum pit_mode mode, unsigned int target_hz)
{
    unsigned int divisor   = PIT_BASE_HZ / target_hz;
    unsigned int remainder = PIT_BASE_HZ % target_hz;
    pr_info("target IRQ freq: %u Hz ~= %u Hz / %u (%u ticks remainder)\n",
            target_hz, PIT_BASE_HZ, divisor, remainder);

    pit_send_cmd(PIT_CH_IRQ, mode, divisor);

    double freq_hz    = (double) PIT_BASE_HZ / divisor;
    double duration_s = 1 / freq_hz;

    pr_info("IRQ freq set: %u Hz / %u ~= " FMT_HZ " (every " FMT_SEC ")\n",
            PIT_BASE_HZ, divisor, freq_hz, duration_s);
    return divisor;
}

