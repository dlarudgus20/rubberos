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

#define PIC_INT_VECTOR      0x20

void pic_mark_irq_as_ready(uint8_t irq);
void pic_send_eoi(uint8_t irq);
