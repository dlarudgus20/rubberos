#pragma once

#include "gui/graphic.h"

enum window_message {
    WM_PAINT,
};

struct window {
    const char* title;
    struct rect rect;
    color_t bg_color;

    struct rect invalidated;

    void (*proc)(struct window* w, enum window_message msg, void* param);
    void* data;
};

void gui_init(void);
void gui_mouse_move(int dx, int dy);
void gui_draw_all(void);

struct window* window_new(void);
struct size window_size_for_client(int width, int height);
void window_invalidate(struct window* w, const struct rect* rt);
void window_redraw(struct window* w, const struct rect* rt);
