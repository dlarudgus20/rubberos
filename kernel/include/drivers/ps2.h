#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "keycode.h"

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

struct ps2_mouse {
    uint8_t packet_index;
    uint8_t packet[3];
};

struct ps2_mouse_event {
    int16_t dx, dy;
    bool left:1, right:1, middle:1;
};

void ps2_keyboard_init(struct ps2_keyboard* kb);
bool ps2_keyboard_put_byte(struct ps2_keyboard* kb, uint8_t byte, struct ps2_keyevent* evt);
struct ps2_char ps2_keyboard_process_keyevent(struct ps2_keyboard* kb, const struct ps2_keyevent* evt);

void ps2_mouse_init(struct ps2_mouse* ms);
bool ps2_mouse_put_byte(struct ps2_mouse* ms, uint8_t byte, struct ps2_mouse_event* evt);
