#pragma once

#include <collections/singlylist.h>
#include "spinlock.h"

struct tty_device {
    struct singlylist_link link;
    void (*write)(struct tty_device* device, const char* str);
    void (*flush)(struct tty_device* device);
};

struct tty {
    struct intrlock lock;
    struct singlylist devices;
    char input_buffer[256];
    unsigned input_index;
};

#if __has_attribute(format)
#define TTY_PRINTF_ATTRIB __attribute__((format(printf, 2, 3)))
#else
#define TTY_PRINTF_ATTRIB
#endif

extern struct tty g_tty0;

void tty0_init(void);

void tty_init(struct tty* tty);
void tty_register_device(struct tty* tty, struct tty_device* device);
void tty_unregister_device(struct tty* tty, struct tty_device* device);

void tty_puts(struct tty* tty, const char* str);
void tty_printf(struct tty* tty, const char* fmt, ...) TTY_PRINTF_ATTRIB;
void tty_on_read(struct tty* tty, char ch);

#define tty0_printf(...) tty_printf(&g_tty0, __VA_ARGS__)
#define tty0_puts(str) tty_puts(&g_tty0, str)
