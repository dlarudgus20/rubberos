#include <stdarg.h>
#include <freec/stdio.h>
#include <freec/stdlib.h>
#include <freec/assert.h>

#include "tty.h"
#include "drivers/framebuffer.h"
#include "drivers/serial.h"
#include "arch/inst.h"

struct tty g_tty0;
static struct tty_device g_ttyd_serial;

static void tty_puts_nolock(struct tty* tty, const char* str);

void tty0_init(void) {
    tty_init(&g_tty0);
    serial_tty_device_init(&g_ttyd_serial);
    tty_register_device(&g_tty0, &g_ttyd_serial);
}

noreturn void panic_impl(const char* msg, const char* file, const char* func, unsigned line) {
    static bool tried = false;
    if (!tried) {
        intrlock_acquire(&g_tty0.lock);
        tried = true;
    }

    char buf[1024];
    snprintf(buf, sizeof(buf), "[%s:%s:%d] %s\n", file, func, line, msg);
    tty_puts_nolock(&g_tty0, buf);
    linkedlist_foreach(ptr, &g_tty0.devices) {
        struct tty_device* device = container_of(ptr, struct tty_device, link);
        if (device->flush) {
            device->flush(device);
        }
    }

    while (1) wait_for_intr();
}

void tty_init(struct tty* tty) {
    intrlock_init(&tty->lock);
    linkedlist_init(&tty->devices);
    tty->input_buffer[0] = 0;
    tty->input_index = 0;
}

void tty_register_device(struct tty* tty, struct tty_device* device) {
    intrlock_acquire(&tty->lock);
    linkedlist_push_back(&tty->devices, &device->link);
    intrlock_release(&tty->lock);
}

void tty_unregister_device(struct tty* tty, struct tty_device* device) {
    intrlock_acquire(&tty->lock);
    linkedlist_foreach(ptr, &tty->devices) {
        if (ptr == &device->link) {
            linkedlist_remove(ptr);
            break;
        }
    }
    intrlock_release(&tty->lock);
}

void tty_puts(struct tty* tty, const char* str) {
    intrlock_acquire(&tty->lock);
    tty_puts_nolock(tty, str);
    intrlock_release(&tty->lock);
}

static void tty_puts_nolock(struct tty* tty, const char* str) {
    linkedlist_foreach(ptr, &tty->devices) {
        struct tty_device* device = container_of(ptr, struct tty_device, link);
        device->write(device, str);
    }
}

void tty_printf(struct tty* tty, const char* fmt, ...) {
    va_list va;
    char buf[1024];
    va_start(va, fmt);
    vsnprintf(buf, sizeof(buf), fmt, va);
    tty_puts(tty, buf);
    va_end(va);
}

void tty_on_read(struct tty* tty, char ch) {
    intrlock_acquire(&tty->lock);
    if (tty->input_index < sizeof(tty->input_buffer) - 1) {
        tty->input_buffer[tty->input_index++] = ch;
        tty->input_buffer[tty->input_index] = 0;
    }
    intrlock_release(&tty->lock);
}
