#pragma once

#include <stdint.h>

#define PIC_IRQ_TIMER       0
#define PIC_IRQ_KEYBOARD    1
#define PIC_IRQ_SLAVE       2
#define PIC_IRQ_SERIAL1     3
#define PIC_IRQ_SERIAL2     4
#define PIC_IRQ_PARALLEL1   5
#define PIC_IRQ_FLOPPY      6
#define PIC_IRQ_PARALLEL2   7
#define PIC_IRQ_RTC         8
#define PIC_IRQ_MOUSE       12
#define PIC_IRQ_COPROC      13
#define PIC_IRQ_HDD1        14
#define PIC_IRQ_HDD2        15

#define PIC_MASK_TIMER      (1 << PIC_IRQ_TIMER)
#define PIC_MASK_KEYBOARD   (1 << PIC_IRQ_KEYBOARD)
#define PIC_MASK_SLAVE      (1 << PIC_IRQ_SLAVE)
#define PIC_MASK_SERIAL1    (1 << PIC_IRQ_SERIAL1)
#define PIC_MASK_SERIAL2    (1 << PIC_IRQ_SERIAL2)
#define PIC_MASK_PARALLEL1  (1 << PIC_IRQ_PARALLEL1)
#define PIC_MASK_FLOPPY     (1 << PIC_IRQ_FLOPPY)
#define PIC_MASK_PARALLEL2  (1 << PIC_IRQ_PARALLEL2)
#define PIC_MASK_RTC        (1 << PIC_IRQ_RTC)
#define PIC_MASK_MOUSE      (1 << PIC_IRQ_MOUSE)
#define PIC_MASK_COPROC     (1 << PIC_IRQ_COPROC)
#define PIC_MASK_HDD1       (1 << PIC_IRQ_HDD1)
#define PIC_MASK_HDD2       (1 << PIC_IRQ_HDD2)

#define PIC_INT_VECTOR      0x20

void pic_mask_int(uint16_t mask);
void pic_send_eoi(uint8_t irq);
