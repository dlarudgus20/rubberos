#pragma once

#include <collections/linkedlist.h>
#include <collections/singlylist.h>
#include <slab/slab.h>

#include "spinlock.h"
#include "./gui.h"
#include "./graphic.h"

struct window {
    struct linkedlist_link link;
    struct singlylist_link draw_link;

    const char* title;
    struct rect scr_rect;
    color_t bg_color;

    bool moving;

    struct rect win_invalidated;

    void (*proc)(struct window* w, enum window_message msg, void* param);
    void* data;
};

/*enum mouse_cursor_type {
    MOUSE_CURSOR_ARROW,
    MOUSE_CURSOR_SIZENS,
    MOUSE_CURSOR_SIZEWE,
    MOUSE_CURSOR_SIZENWSE,
    MOUSE_CURSOR_SIZENESW,
};*/

struct winman {
    struct intrlock lock;
    struct slab_allocator slab_window;

    struct graphic backbuffer;
    bool painting;

    struct rect sizing_rect;
    struct point sizing_pt;
    bool sizing;

    struct linkedlist window_list;
    struct window* focused;
    struct window* mouse_capturing;

    struct size scr_size;
    struct point mouse_pos;
    //uint8_t mouse_cursor_type;

    struct rect global_invalidated;
    color_t bg_color;
};

extern struct winman g_winman;
