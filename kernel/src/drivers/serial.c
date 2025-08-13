#include <stddef.h>
#include <stdarg.h>
#include <freec/stdio.h>

#include "drivers/serial.h"
#include "tty.h"

void serial_puts(const char* str) {
    while (*str) {
        serial_putchar(*str++);
    }
}

static void write(struct tty_device* device, const char* str) {
    serial_puts(str);
}

void serial_tty_device_init(struct tty_device* device) {
    device->write = write;
    device->flush = NULL;
}
