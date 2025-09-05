#pragma once

#include <stdint.h>

#include "./gui.h"
#include "drivers/hid.h"

#define MOUSE_EVENT_NONE    0
#define MOUSE_EVENT_DOWN    1
#define MOUSE_EVENT_UP      2

struct gui_mouse_event {
    int x, y;
    bool prev_left:1, prev_right:1, prev_middle:1;
    bool left:1, right:1, middle:1;
};

#define gui_mouse_event_down(evt, btn)  ((evt).btn && !(evt).prev_##btn)
#define gui_mouse_event_up(evt, btn)    (!(evt).btn && (evt).prev_##btn)

void gui_on_keyboard(struct hid_keyevent evt, struct hid_char c);
void gui_on_mouse(struct hid_mouse_event evt);

// window.c
void window_sendmsg_focused(enum window_message msg, void* param);
void window_sendmsg_mouse(int x, int y, struct gui_mouse_event evt);
