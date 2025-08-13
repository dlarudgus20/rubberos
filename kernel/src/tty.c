#include <stdarg.h>
#include <freec/stdio.h>
#include <freec/stdlib.h>
#include <freec/assert.h>

#include "tty.h"
#include "drivers/framebuffer.h"
#include "drivers/serial.h"

struct tty g_tty0;
static struct tty_device g_ttyd_serial;

void tty0_init(void) {
    tty_init(&g_tty0);
    serial_tty_device_init(&g_ttyd_serial);
    tty_register_device(&g_tty0, &g_ttyd_serial);
}

noreturn void panic_impl(const char* msg, const char* file, const char* func, unsigned line) {
    tty_printf(&g_tty0, "[%s:%s:%d] %s\n", file, func, line, msg);
    /*singlylist_foreach(ptr, &g_tty0.devices) {
        struct tty_device* device = container_of(ptr, struct tty_device, link);
        if (device->flush) {
            device->flush(device);
        }
    }*/
    __asm__ __volatile__ ( "cli" );
    while (1) __asm__ __volatile__ ("hlt");
}

void tty_init(struct tty* tty) {
    singlylist_init(&tty->devices);
    tty->input_buffer[0] = 0;
    tty->input_index = 0;
}

void tty_register_device(struct tty* tty, struct tty_device* device) {
    singlylist_push_front(&tty->devices, &device->link);
}

void tty_unregister_device(struct tty* tty, struct tty_device* device) {
    singlylist_foreach_2(before, ptr, &tty->devices) {
        if (ptr == &device->link) {
            singlylist_remove_after(before);
            break;
        }
    }
}

void tty_puts(struct tty* tty, const char* str) {
    singlylist_foreach(ptr, &tty->devices) {
        struct tty_device* device = container_of(ptr, struct tty_device, link);
        device->write(device, str);
    }
}

void tty_printf(struct tty* tty, const char* fmt, ...) {
    va_list va;
    char buf[4096];
    va_start(va, fmt);
    vsnprintf(buf, sizeof(buf), fmt, va);
    tty_puts(tty, buf);
    va_end(va);
}

void tty_on_read(struct tty* tty, char ch) {
    if (tty->input_index < sizeof(tty->input_buffer) - 1) {
        tty->input_buffer[tty->input_index++] = ch;
        tty->input_buffer[tty->input_index] = 0;
    }
}
