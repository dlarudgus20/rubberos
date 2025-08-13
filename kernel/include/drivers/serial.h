#pragma once

struct tty_device;

void serial_init(void);
void serial_putchar(char ch);
void serial_puts(const char* str);

void serial_tty_device_init(struct tty_device* device);
