#include <stdbool.h>

#include "drivers/hid.h"
#include "drivers/ps2.h"

#include "interrupt.h"
#include "arch/inst.h"
#include "arch/x86_64/pic.h"

#include "tty.h"
#include "gui/gui.h"

#define CTRL   0x64
#define STAT   0x64
#define INPUT  0x60
#define OUTPUT 0x60

#define CTRL_READ_CFG   0x20
#define CTRL_WRITE_CFG  0x60
#define CTRL_DISABLE_2  0xa7
#define CTRL_ENABLE_2   0xa8
#define CTRL_TESTP_2    0xa9
#define CTRL_TEST       0xaa
#define CTRL_TESTP_1    0xab
#define CTRL_DISABLE_1  0xad
#define CTRL_ENABLE_1   0xae
#define CTRL_WRITE_2    0xd4

#define STAT_INPB       0x02
#define STAT_OUTB       0x01

#define CFG_INTR_1      0x01
#define CFG_INTR_2      0x02
#define CFG_CLOCK_1     0x10
#define CFG_CLOCK_2     0x20
#define CFG_TRANSLATE   0x40

#define CMD_ENABLE_SCAN 0xf4
#define CMD_RESET       0xff

#define ACK     0xfa
#define RESEND  0xfe

#define WAIT_REPEAT 100

static struct ps2_keyboard g_kb;
static struct ps2_mouse g_ms;

void isr_keyboard();
void isr_mouse();

static bool input_buffer_is_full(void) {
    return in8(STAT) & STAT_INPB;
}

static bool output_buffer_is_full(void) {
    return in8(STAT) & STAT_OUTB;
}

static void wait_for_input(void) {
    for (int i = 0; i < WAIT_REPEAT; i++) {
        if (!input_buffer_is_full()) {
            return;
        }
    }
}

static void wait_for_output(void) {
    for (int i = 0; i < WAIT_REPEAT; i++) {
        if (output_buffer_is_full()) {
            return;
        }
    }
}

struct device_type {
    uint8_t bytes[2];
    int n;
};

static struct device_type read_device_type_reset(void) {
    wait_for_output();
    if (in8(OUTPUT) != 0xfa) {
        wait_for_input();
        out8(CTRL, CTRL_DISABLE_1);
        return (struct device_type){ .n = -1 };
    }
    wait_for_output();
    if (in8(OUTPUT) != 0xaa) {
        wait_for_input();
        out8(CTRL, CTRL_DISABLE_1);
        return (struct device_type){ .n = -1 };
    }

    struct device_type type = { 0 };
    for (int i = 0; i < WAIT_REPEAT && type.n < 2; i++) {
        if (output_buffer_is_full()) {
            type.bytes[type.n++] = in8(OUTPUT);
        }
    }
    return type;
}

static bool kb_init(void) {
    wait_for_input();
    out8(CTRL, CTRL_ENABLE_1);
    wait_for_input();
    out8(INPUT, CMD_RESET);

    struct device_type type = read_device_type_reset();
    if (type.n != 0) {
        wait_for_input();
        out8(CTRL, CTRL_DISABLE_1);
        return false;
    }

    interrupt_register_isr(PIC_INT_VECTOR + PIC_IRQ_KEYBOARD, isr_keyboard);
    return true;
}

static void kb_enable_scan(void) {
    for (int i = 0; i < 10; i++) {
        wait_for_input();
        out8(INPUT, CMD_ENABLE_SCAN);
        wait_for_output();
        uint8_t ack = in8(OUTPUT);
        if (ack == ACK) {
            break;
        }
    }
}

static bool ms_init(void) {
    // port 2 is already enabled
    wait_for_input();
    out8(CTRL, CTRL_WRITE_2);
    wait_for_input();
    out8(INPUT, CMD_RESET);

    struct device_type type = read_device_type_reset();
    if (!(type.n == 1 && type.bytes[0] == 0x00)) {
        wait_for_input();
        out8(CTRL, CTRL_DISABLE_1);
        return false;
    }

    interrupt_register_isr(PIC_INT_VECTOR + PIC_IRQ_MOUSE, isr_mouse);
    return true;
}

static void ms_enable_scan(void) {
    for (int i = 0; i < 10; i++) {
        wait_for_input();
        out8(CTRL, CTRL_WRITE_2);
        wait_for_input();
        out8(INPUT, CMD_ENABLE_SCAN);
        wait_for_output();
        uint8_t ack = in8(OUTPUT);
        if (ack == ACK) {
            break;
        }
    }
}

void hid_init(void) {
    bool p1 = true, p2 = false;

    ps2_keyboard_init(&g_kb);
    ps2_mouse_init(&g_ms);

    // disable & flush output buffer
    wait_for_input();
    out8(CTRL, CTRL_DISABLE_1);
    wait_for_input();
    out8(CTRL, CTRL_DISABLE_2);
    in8(OUTPUT);

    // test controller
    wait_for_input();
    out8(CTRL, CTRL_TEST);
    wait_for_output();
    if (in8(OUTPUT) != 0x55) {
        return;
    }

    // set config byte
    wait_for_input();
    out8(CTRL, CTRL_WRITE_CFG);
    wait_for_input();
    out8(INPUT, CFG_CLOCK_1 | CFG_CLOCK_2 | CFG_TRANSLATE);

    // detect channel 2
    wait_for_input();
    out8(CTRL, CTRL_ENABLE_2);
    wait_for_input();
    out8(CTRL, CTRL_READ_CFG);
    wait_for_output();
    if (!(in8(OUTPUT) & CFG_CLOCK_2)) {
        p2 = true;
    }

    // test ports
    wait_for_input();
    out8(CTRL, CTRL_TESTP_1);
    wait_for_output();
    if (in8(OUTPUT) != 0x00) {
        p1 = false;
    }
    wait_for_input();
    out8(CTRL, CTRL_TESTP_2);
    wait_for_output();
    if (in8(OUTPUT) != 0x00) {
        p2 = false;
    }

    if (p1) {
        p1 = kb_init();
    }
    if (p2) {
        p2 = ms_init();
    }

    // enable interrupt
    if (p1) {
        pic_irq_ready(PIC_IRQ_KEYBOARD);
        kb_enable_scan();
    }
    if (p2) {
        pic_irq_ready(PIC_IRQ_MOUSE);
        ms_enable_scan();
    }

    uint8_t cfg = CFG_TRANSLATE;
    cfg |= p1 ? CFG_INTR_1 : CFG_CLOCK_1;
    cfg |= p2 ? CFG_INTR_2 : CFG_CLOCK_2;
    wait_for_input();
    out8(CTRL, CTRL_WRITE_CFG);
    wait_for_input();
    out8(INPUT, cfg);
}

void isr_impl_keyboard(struct isr_stackframe* frame) {
    uint8_t code = in8(OUTPUT);
    intr_queue_push(&(struct intr_msg){ .type = INTR_MSG_KEYBOARD, .data = code });
    pic_send_eoi(PIC_IRQ_KEYBOARD);
}

void isr_impl_mouse(struct isr_stackframe* frame) {
    uint8_t code = in8(OUTPUT);
    intr_queue_push(&(struct intr_msg){ .type = INTR_MSG_MOUSE, .data = code });
    pic_send_eoi(PIC_IRQ_MOUSE);
}

void intr_msg_on_keyboard(const struct intr_msg* msg) {
    struct ps2_keyevent evt;
    if (ps2_keyboard_put_byte(&g_kb, msg->data, &evt)) {
        struct ps2_char c = ps2_keyboard_process_keyevent(&g_kb, &evt);
        if (!c.raw) {
            tty_puts(&g_tty0, (const char[2]){ c.ch, 0 });
        }
    }
}

void intr_msg_on_mouse(const struct intr_msg* msg) {
    struct ps2_mouse_event evt;
    if (ps2_mouse_put_byte(&g_ms, msg->data, &evt)) {
        gui_mouse_move(evt.dx, -evt.dy);
    }
}
