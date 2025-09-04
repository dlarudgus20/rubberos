#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hid.h"

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

struct ps2_mouse {
    uint8_t packet_index;
    uint8_t packet[3];
};

void ps2_keyboard_init(struct ps2_keyboard* kb);
bool ps2_keyboard_put_byte(struct ps2_keyboard* kb, uint8_t byte, struct hid_keyevent* evt);
struct hid_char ps2_keyboard_process_keyevent(struct ps2_keyboard* kb, const struct hid_keyevent* evt);

void ps2_mouse_init(struct ps2_mouse* ms);
bool ps2_mouse_put_byte(struct ps2_mouse* ms, uint8_t byte, struct hid_mouse_event* evt);
