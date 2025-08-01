#include "kmain.h"

#include "drivers/serial.h"
#include "drivers/framebuffer.h"
#include "arch/inst.h"

#include "memory.h"

void kmain(void) {
    serial_init();
    fb_init();

    mmap_print();

    while (1) {
        wait_for_int();
    }
}
