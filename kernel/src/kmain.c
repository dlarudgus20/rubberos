#include "kmain.h"

#include "interrupt.h"
#include "arch/inst.h"
#include "drivers/serial.h"
#include "drivers/framebuffer.h"
#include "drivers/hid.h"

#include "memory.h"
#include "tty.h"
#include "gui/gui.h"
#include "gui/tty_window.h"
#include "gui/window.h"

static void dispatch_intr_msg(struct intr_msg* msg) {
    switch (msg->type) {
        case INTR_MSG_KEYBOARD:
            intr_msg_on_keyboard(msg);
            break;
        case INTR_MSG_MOUSE:
            intr_msg_on_mouse(msg);
            break;
    }
}

void kmain(void) {
    interrupt_init();
    serial_init();
    tty0_init();
    memory_init();
    fb_init();
    gui_init();

    struct tty_window tw;
    tty_window_init(&tw, &g_tty0);

    struct window* w1 = window_new();
    w1->scr_rect = (struct rect){ 50, 50, 400, 300 };
    struct window* w2 = window_new();
    w2->scr_rect = (struct rect){ 75, 75, 400, 300 };

    interrupt_device_init();
    hid_init();
    interrupt_device_enable();

    mmap_print_bootinfo();
    mmap_print_dyn();
    //pagetable_print();
    dynmem_print();

    gui_draw_all();

    struct intr_msg msg;
    while (1) {
        interrupt_enable_and_wait();

        while (1) {
            interrupt_disable();
            compiler_barrier();
            if (!intr_queue_try_pop(&msg)) {
                break;
            }

            interrupt_enable();
            dispatch_intr_msg(&msg);
        }

        gui_draw_all();
    }
}
