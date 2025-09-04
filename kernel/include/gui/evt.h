#pragma once

#include "./gui.h"
#include "drivers/hid.h"

struct gui_mouse_event {
    int x, y;
    bool left:1, right:1, middle:1;
};

void gui_on_keyboard(struct hid_keyevent evt, struct hid_char c);
void gui_on_mouse(struct hid_mouse_event evt);

// window.c
void window_sendmsg_focused(enum window_message msg, void* param);
void window_sendmsg_mouse(int x, int y, struct gui_mouse_event evt);
