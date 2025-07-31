#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdnoreturn.h>

#include <freec/stdio.h>
#include <freec/assert.h>

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

void printf_serial(const char *format, ...)
{
    va_list va;
    char buf[4096];
    va_start(va, format);
    vsnprintf(buf, sizeof(buf), format, va);
    for (int i = 0; buf[i] != 0; i++) {
        putchar_serial(buf[i]);
    }
    va_end(va);
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
    printf_serial("1+2=%d (0x%04x), 1*2=%d (0x%04x)\n", 1+2, 1+2, 1*2, 1*2);
    while (1) __asm__ __volatile__ ("hlt");
}

noreturn void panic_impl(const char* msg, const char* file, const char* func, unsigned line) {
    printf_serial("[%s:%s:%d] %s\n", file, func, line, msg);
    __asm__ __volatile__ ( "cli" );
    while (1) __asm__ __volatile__ ("hlt");
}
