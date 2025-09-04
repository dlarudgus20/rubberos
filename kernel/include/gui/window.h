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

    struct rect win_invalidated;

    void (*proc)(struct window* w, enum window_message msg, void* param);
    void* data;
};

struct winman {
    struct intrlock lock;
    struct slab_allocator slab_window;
    bool painting;

    struct linkedlist window_list;
    struct window* focused;

    struct size scr_size;
    struct point mouse_pos;

    struct rect global_invalidated;
    color_t bg_color;
};

extern struct winman g_winman;
