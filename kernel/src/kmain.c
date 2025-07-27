#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "buddy.h"
#include "slab_alloc.h"

void out8(uint16_t port, uint8_t data) {
    __asm__ __volatile__ ( "out dx, al" : : "Nd"(port), "a"(data) : "memory" );
}

uint8_t in8(uint16_t port) {
    uint8_t data;
    __asm__ __volatile__ ( "in al, dx" : "=a"(data) : "Nd"(port) : "memory" );
    return data;
}

void init_serial(void) {
    out8(0x3f8 + 1, 0x00);
    out8(0x3f8 + 3, 0x80);
    out8(0x3f8 + 0, 0x03);
    out8(0x3f8 + 1, 0x00);
    out8(0x3f8 + 3, 0x03);
    out8(0x3f8 + 2, 0xc7);
    out8(0x3f8 + 4, 0x0b);
    out8(0x3f8 + 1, 0x01);
}

void putchar_serial(char ch) {
    while ((in8(0x3f8 + 5) & 0x20) == 0) {}
    out8(0x3f8, ch);
}

void tohex(char *buf, uint32_t val) {
    const char *str = "0123456789ABCDEF";
    for (int i = 7; i >= 0; i--) {
        buf[i] = str[val & 0xf];
        val >>= 4;
    }
}

void printf_serial(const char *fmt, ...) {
    va_list args;
    char buf[8];
    va_start(args, fmt);
    for (; *fmt != 0; fmt++) {
        switch (*fmt) {
            case '%':
                switch (*(++fmt)) {
                    case 0:
                        putchar_serial('%');
                        goto end;
                    case 'x':
                        tohex(buf, va_arg(args, uint32_t));
                        putchar_serial('0');
                        putchar_serial('x');
                        for (int i = 0; i < 8; i++) {
                            putchar_serial(buf[i]);
                        }
                        continue;
                    default:
                        putchar_serial('%');
                        putchar_serial(*fmt);
                        continue;
                }
            default:
                putchar_serial(*fmt);
                break;
        }
    }
end:
    putchar_serial('\n');
    va_end(args);
}

void kmain(volatile int* video, int width, int height) {
    init_serial();
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int color = x ^ y;
            if (y < 10 || y >= height - 10 || x < 10 || x >= width - 10) {
                color = 0x0000ff00;
            }
            video[x + y * width] = color;
        }
    }
    printf_serial("1+2=%x, 1*2=%x", buddy_add(1, 2), slab_mul(1, 2));
    while (1) __asm__ __volatile__ ("hlt");
}
