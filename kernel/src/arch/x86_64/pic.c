#include "arch/x86_64/pic.h"

#include "interrupt.h"
#include "arch/inst.h"

#define MASTER1 0x20
#define MASTER2 0x21
#define SLAVE1 0xa0
#define SLAVE2 0xa1

static uint16_t g_mask = ~(1 << PIC_IRQ_SLAVE);

static void set_mask(uint16_t mask) {
    out8(MASTER2, (uint8_t)mask);
    out8(SLAVE2, (uint8_t)(mask >> 8));
}

void interrupt_device_init(void) {
    out8(MASTER1, 0x11);
    out8(MASTER2, PIC_INT_VECTOR);
    out8(MASTER2, 0x04);
    out8(MASTER2, 0x01);
    out8(SLAVE1, 0x11);
    out8(SLAVE2, PIC_INT_VECTOR + 8);
    out8(SLAVE2, 0x02);
    out8(SLAVE2, 0x01);
    set_mask(g_mask);
}
#include "tty.h"
void pic_irq_ready(uint8_t irq) {
    tty0_printf("pic_irq_ready: %d\n", irq);
    g_mask &= ~(1 << irq);
}

void interrupt_device_enable(void) {
    tty0_printf("device_enable: %#06x\n", g_mask);
    set_mask(g_mask);
}

void pic_send_eoi(uint8_t irq) {
    if (irq < 8) {
        out8(MASTER1, 0x60 | irq);
    } else {
        out8(MASTER1, 0x60 | PIC_IRQ_SLAVE);
        out8(SLAVE1, 0x60 | (irq - 8));
    }
}
