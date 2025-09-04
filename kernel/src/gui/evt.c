#include "gui/evt.h"
#include "drivers/hid.h"

void gui_on_keyboard(struct hid_keyevent evt, struct hid_char c) {
    window_sendmsg_focused(WM_KEY, &evt);
    if (!c.raw) {
        window_sendmsg_focused(WM_CHAR, &c);
    }
}

void gui_on_mouse(struct hid_mouse_event evt) {
    struct point pos = gui_mouse_move(evt.dx, -evt.dy);
    struct gui_mouse_event ge = {
        .x = pos.x,
        .y = pos.y,
        .left = evt.left,
        .right = evt.right,
        .middle = evt.middle,
    };
    window_sendmsg_mouse(pos.x, pos.y, ge);
}
