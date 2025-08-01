#include "kmain.h"

#include "drivers/serial.h"
#include "drivers/framebuffer.h"

void kmain(void) {
    serial_init();
    fb_init();
    serial_printf("1+2=%d (0x%04x), 1*2=%d (0x%04x)\n", 1+2, 1+2, 1*2, 1*2);
    while (1) __asm__ __volatile__ ("hlt");
}
