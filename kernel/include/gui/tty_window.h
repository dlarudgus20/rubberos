#pragma once

#include <stdint.h>
#include "tty.h"

struct window;

#define TTYW_WIDTH 80
#define TTYW_HEIGHT 25

struct tty_window {
    struct tty_device device;
    struct window* window;
    char buffer[TTYW_WIDTH * TTYW_HEIGHT];
    uint16_t cursor_x;
    uint16_t cursor_y;
};

void tty_window_init(struct tty_window* tw, struct tty* tty);
