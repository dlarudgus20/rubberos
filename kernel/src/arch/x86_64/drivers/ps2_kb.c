#include <stdbool.h>

#include "drivers/keyboard.h"
#include "drivers/ps2.h"

#include "interrupt.h"
#include "arch/inst.h"
#include "arch/x86_64/pic.h"

#include "tty.h"

#define CTRL   0x64
#define STAT   0x64
#define INPUT  0x60
#define OUTPUT 0x60

#define CTRL_ACTIVATE_MOUSE     0xa7
#define CTRL_DEACTIVATE_MOUSE   0xa8
#define CTRL_ACTIVATE_KB        0xae
#define CTRL_DEACTIVATE_KB      0xae
#define CTRL_READ_OUTP          0xd0
#define CTRL_WRITE_OUTP         0xd1

#define STAT_PARE   0x80
#define STAT_TIM    0x40
#define STAT_AUXB   0x20
#define STAT_KEYL   0x10
#define STAT_C_D    0x08
#define STAT_SYSF   0x04
#define STAT_INPB   0x02
#define STAT_OUTB   0x01

#define CMD_LED         0xed
#define CMD_ACTIVATE    0xf4

#define ACK 0xfa

static struct ps2_keyboard g_ps2;

void isr_keyboard();

static bool input_buffer_is_full(void) {
    return in8(STAT) & STAT_INPB;
}

static bool output_buffer_is_full(void) {
    return in8(STAT) & STAT_OUTB;
}

static void wait_for_input(void) {
    for (int i = 0; i < 0xffff; i++) {
        if (!input_buffer_is_full()) {
            return;
        }
    }
}

static bool wait_for_ack(void) {
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 0xffff; j++) {
            if (output_buffer_is_full()) {
                break;
            }
        }
        if (in8(OUTPUT) == ACK) {
            return true;
        }
    }
    return false;
}

void keyboard_init(void) {
    ps2_keyboard_init(&g_ps2);

    out8(CTRL, CTRL_ACTIVATE_KB);
    wait_for_input();
    out8(INPUT, CMD_ACTIVATE);
    if (!wait_for_ack()) {
        return;
    }

    interrupt_register_isr(PIC_INT_VECTOR + PIC_IRQ_KEYBOARD, isr_keyboard);
}

void isr_impl_keyboard(struct isr_stackframe* frame) {
    uint8_t code = in8(OUTPUT);
    if (code != ACK) {
        intr_queue_push(&(struct intr_msg){ .type = INTR_MSG_KEYBOARD, .data = code });
    }

    pic_send_eoi(PIC_IRQ_KEYBOARD);
}

#include "gui/gui.h"

void intr_msg_on_keyboard(const struct intr_msg* msg) {
    struct ps2_keyevent evt;
    if (ps2_keyboard_put_byte(&g_ps2, msg->data, &evt)) {
        struct ps2_char c = ps2_keyboard_process_keyevent(&g_ps2, &evt);
        if (c.raw) {
            tty0_printf("ke:%#04x", c.keycode);
        } else {
            tty0_printf("ke:<%c>", c.ch);
        }
        gui_draw_all();
    }
}
