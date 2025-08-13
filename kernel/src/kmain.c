#include "kmain.h"

#include "arch/interrupt.h"
#include "arch/inst.h"
#include "drivers/serial.h"
#include "drivers/framebuffer.h"

#include "memory.h"
#include "tty.h"
#include "gui/gui.h"
#include "gui/tty_window.h"

void kmain(void) {
    interrupt_init();
    serial_init();
    tty0_init();
    memory_init();
    fb_init();
    gui_init();

    struct tty_window tw;
    tty_window_init(&tw, &g_tty0);

    mmap_print_bootinfo();
    mmap_print_dyn();
    pagetable_print();
    dynmem_print();

    gui_draw_all();

    __asm__ __volatile__ ( "int 0" );

    while (1) {
        wait_for_int();
    }
}
