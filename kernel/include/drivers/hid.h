#pragma once

#include "interrupt.h"
#include "keycode.h"

struct hid_keyevent {
    keycode_t keycode;
    bool keydown;
};

struct hid_char {
    bool raw;
    char ch;
    keycode_t keycode;
};

struct hid_mouse_event {
    int16_t dx, dy;
    bool left:1, right:1, middle:1;
};

void hid_init(void);

void hid_on_keyboard(struct hid_keyevent evt, struct hid_char c);
void hid_on_mouse(struct hid_mouse_event evt);

void intr_msg_on_keyboard(const struct intr_msg* msg);
void intr_msg_on_mouse(const struct intr_msg* msg);
