#pragma once

#include "./graphic.h"

enum window_message {
    WM_PAINT,
    WM_MOUSE,
    WM_KEY,
    WM_CHAR,
};

struct window;

void gui_init(void);

struct window* window_new(void);

struct size window_size_for_client(int width, int height);
struct rect client_rect_for_window(int width, int height);

void gui_draw_all(void);
void window_invalidate(struct window* w, const struct rect* rt);
void window_redraw(struct window* w, const struct rect* rt);

void window_set_focus(struct window* w);

struct point gui_mouse_move(int dx, int dy);
