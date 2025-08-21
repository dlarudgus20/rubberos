#include <stdarg.h>
#include <freec/stdio.h>

#include "drivers/serial.h"
#include "arch/inst.h"

void serial_init(void) {
    out8(0x3f8 + 1, 0x00);
    out8(0x3f8 + 3, 0x80);
    out8(0x3f8 + 0, 0x03);
    out8(0x3f8 + 1, 0x00);
    out8(0x3f8 + 3, 0x03);
    out8(0x3f8 + 2, 0xc7);
    out8(0x3f8 + 4, 0x0b);
    out8(0x3f8 + 1, 0x01);
}

static void real_putchar(char ch) {
    while ((in8(0x3f8 + 5) & 0x20) == 0) {}
    out8(0x3f8, ch);
}

void serial_putchar(char ch) {
    unsigned char x = (unsigned char)ch;
    const char* digits = "0123456789abcdef";
    real_putchar(digits[x >> 4]);
    real_putchar(digits[x & 0xf]);
    real_putchar(' ');
}
