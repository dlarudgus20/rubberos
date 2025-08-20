#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "keyboard/keycode.h"

struct ps2_keyboard {
    uint8_t state;
    uint8_t data;
    bool lshift:1;
    bool rshift:1;
    bool lctrl:1;
    bool rctrl:1;
    bool lalt:1;
    bool ralt:1;
    bool caps:1;
    bool scroll:1;
    bool num:1;
};

struct ps2_keyevent {
    keycode_t keycode;
    bool keydown;
};

struct ps2_char {
    bool raw;
    char ch;
    keycode_t keycode;
};

void ps2_keyboard_init(struct ps2_keyboard* kb);
bool ps2_keyboard_put_byte(struct ps2_keyboard* kb, uint8_t byte, struct ps2_keyevent* evt);
struct ps2_char ps2_keyboard_process_keyevent(struct ps2_keyboard* kb, const struct ps2_keyevent* evt);
