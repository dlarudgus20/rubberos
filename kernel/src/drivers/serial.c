#include <stdarg.h>
#include <freec/stdio.h>
#include <freec/assert.h>

#include "drivers/serial.h"

void serial_printf(const char *format, ...) {
    va_list va;
    char buf[4096];
    va_start(va, format);
    vsnprintf(buf, sizeof(buf), format, va);
    for (int i = 0; buf[i] != 0; i++) {
        serial_putchar(buf[i]);
    }
    va_end(va);
}

noreturn void panic_impl(const char* msg, const char* file, const char* func, unsigned line) {
    serial_printf("[%s:%s:%d] %s\n", file, func, line, msg);
    __asm__ __volatile__ ( "cli" );
    while (1) __asm__ __volatile__ ("hlt");
}
