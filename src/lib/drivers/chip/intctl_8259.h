/**
 * @file
 *
 * Driver for Intel 8259 Interrupt Controller
 *
 * References:
 *
 * - OSDev Wiki: <https://wiki.osdev.org/8259_PIC>
 * - Intel 8259 datasheet:
 *      <https://pdos.csail.mit.edu/6.828/2005/readings/hardware/8259A.pdf>
 *
 * Apologies for the outdated "master/slave" terminology. We are using it here
 * to match the original datasheet and documentation.
 */
#ifndef INTCTL_8259_H
#define INTCTL_8259_H

#include <core/compiler.h>

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t  irq_t;     // IRQ number
typedef uint16_t irqmask_t; // IRQ bitmask

#define IRQ_TIMER    0
#define IRQ_KEYBOARD 1
#define IRQ_CASCADE  2 // Cascaded IRQ from the slave PIC
#define IRQ_RTC      8
#define IRQ_MAX      16

#define IRQ_MASTER_LOWEST_PRIORITY 7
#define IRQ_SLAVE_START            8
#define IRQ_SLAVE_LOWEST_PRIORITY  15

/* Initialization */

int pic_init(unsigned char irq_start);

/* Mask/unmask individual IRQs */

void pic_mask_irq(irq_t irq);
void pic_unmask_irq(irq_t irq);

/* Get/set the bitmask for all 16 IRQs */

irqmask_t pic_get_mask(void);
void      pic_set_mask(irqmask_t mask);

/* Other functions */

int pic_send_eoi(irq_t irq);

ATTR_CALLED_FROM_ISR
bool pic_check_spurious(irq_t irq);

#endif /* INTCTL_8259_H */
