/**
 * @file
 *
 * Driver for Intel 8253 Programmable Interval Timer (PIT)
 *
 * References:
 *
 * - OSDev Wiki: <https://wiki.osdev.org/Programmable_Interval_Timer>
 * - Bran's Kernel Development Tutorial:
 *      <http://www.osdever.net/bkerndev/Docs/pit.htm>
 */
#ifndef PIT_8235_H
#define PIT_8235_H

#include <cpu.h>

#include <stdbool.h>
#include <stdint.h>

static const unsigned int PIT_PORT_CH0      = 0x40;
static const unsigned int PIT_PORT_CH1      = 0x41;
static const unsigned int PIT_PORT_CH2      = 0x42;
static const unsigned int PIT_PORT_MODE_CMD = 0x43;

static const unsigned int PIT_BASE_HZ = 1193180;

enum pit_channel {
    PIT_CH_IRQ     = 0,
    PIT_CH1        = 1,
    PIT_CH_SPEAKER = 2,
};

enum pit_mode {
    PIT_MODE_INTERRUPT_ON_COUNT = 0,
    PIT_MODE_ONESHOT            = 1,
    PIT_MODE_RATE_GEN           = 2,
    PIT_MODE_SQUARE_WAVE        = 3,
    PIT_MODE_SOFTWARE_STROBE    = 4,
    PIT_MODE_HARDWARE_STROBE    = 5,
};

enum pit_rw {
    PIT_RW_LSB     = 1,
    PIT_RW_MSB     = 2,
    PIT_RW_LSB_MSB = 3,
};

enum pit_bcd {
    PIT_BASE2 = 0, // Data is a 16-bit counter
    PIT_BCD   = 1, // Data is four BCD decade counters
};

static inline uint8_t pit_cmd_byte(
        enum pit_channel cntr,
        enum pit_rw      rw,
        enum pit_mode    mode,
        enum pit_bcd     bcd
)
{
    return cntr << 6 | rw << 4 | mode << 1 | bcd;
}

void pit_send_cmd(
        enum pit_channel channel, enum pit_mode mode, uint16_t count
);

unsigned int pit_set_irq_freq(enum pit_mode mode, unsigned int target_hz);

/* === PC Speaker === */

#define PCSPKR_PORT 0x61

//                          bit 76543210
#define PCSPKR_CMD_MASK       0b00000011
#define PCSPKR_CMD_CTL_MANUAL 0b00000000
#define PCSPKR_CMD_CTL_PIT    0b00000001
#define PCSPKR_CMD_POS_IN     0b00000000
#define PCSPKR_CMD_POS_OUT    0b00000010
#define PCSPKR_RD_TIMER_STATE 0b00100000

uint8_t pcspkr_read(void);
uint8_t pcspkr_send_cmd(uint8_t flags);
void    pcspkr_restore(uint8_t prev_state);

#endif /* PIT_8235_H */
