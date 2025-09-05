#include "gui/evt.h"
#include "gui/window.h"
#include "drivers/hid.h"

void gui_on_keyboard(struct hid_keyevent evt, struct hid_char c) {
    window_sendmsg_focused(WM_KEY, &evt);
    if (!c.raw) {
        window_sendmsg_focused(WM_CHAR, &c);
    }
}

void gui_on_mouse(struct hid_mouse_event evt) {
    static bool prev_left = false, prev_right = false, prev_middle = false;

    struct point pos = gui_mouse_move(evt.dx, -evt.dy);
    struct gui_mouse_event ge = {
        .x = pos.x,
        .y = pos.y,
        .prev_left = prev_left,
        .prev_right = prev_right,
        .prev_middle = prev_middle,
        .left = evt.left,
        .right = evt.right,
        .middle = evt.middle,
    };
    prev_left = evt.left;
    prev_right = evt.right;
    prev_middle = evt.middle;

    window_sendmsg_mouse(pos.x, pos.y, ge);
}
