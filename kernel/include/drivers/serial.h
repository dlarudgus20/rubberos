#pragma once

#if __has_attribute(format)
#define SERIAL_PRINTF_ATTRIB __attribute__((format(printf, 1, 2)))
#else
#define SERIAL_PRINTF_ATTRIB
#endif

void serial_init(void);
void serial_putchar(char ch);
void serial_printf(const char *format, ...) SERIAL_PRINTF_ATTRIB;
