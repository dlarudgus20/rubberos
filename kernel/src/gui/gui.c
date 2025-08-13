#include <slab/slab.h>
#include <collections/arraylist.h>
#include <freec/stdlib.h>
#include "gui/gui.h"
#include "gui/graphic.h"
#include "memory.h"

struct gui {
    struct slab_allocator slab_window;
    struct arraylist draw_list;
};

static struct gui g_gui;

void gui_init(void) {
    SLAB_INIT(&g_gui.slab_window, struct window);
    arraylist_init(&g_gui.draw_list, 0, &g_arraylist_allocator);
}

struct window* window_new(void) {
    struct window* w = slab_alloc(&g_gui.slab_window);
    w->title = "New Window";
    w->rect = (struct rect){ .x = 120, .y = 120, .width = 640, .height = 480 };
    w->bg_color = 0xffffff;
    *(struct window**)arraylist_push_back(&g_gui.draw_list, sizeof(struct window*)) = w;
    return w;
}

#define BORDER1 1
#define BORDER2 2
#define TITLE 20

static void draw_window(struct window* w, struct graphic* g) {
    graphic_draw_rect(g, 0, 0, w->rect.width, w->rect.height, BORDER1, 0x2f2f2f);
    graphic_draw_rect(g, BORDER1, BORDER1, w->rect.width - 2 * BORDER1, w->rect.height - 2 * BORDER1, BORDER2, 0x3f3f3f);

    const int border = BORDER1 + BORDER2;
    const int inborder_width = w->rect.width - 2 * border;
    const int inborder_height = w->rect.height - 2 * border;
    const int title = inborder_height > TITLE ? TITLE : inborder_height;

    graphic_fill_rect(g, border, border, inborder_width, title, 0x5f5f5f);

    struct rect rt = { border + TITLE, border + 2, inborder_width - TITLE, 16 };
    graphic_draw_string(g, &rt, w->title, 0x000000, false);

    struct rect client = { border, border + TITLE, inborder_width, inborder_height - TITLE };
    graphic_fill_rect(g, client.x, client.y, client.width, client.height, g->bg_color);

    if (w->proc) {
        graphic_set_offset(g, &(struct rect){
            client.x + g->offset.x, client.y + g->offset.y, client.width, client.height });
        w->proc(w, WM_PAINT, g);
    }
}

struct size window_size_for_client(int width, int height) {
    return (struct size){ width + 2 * (BORDER1 + BORDER2), height + 2 * (BORDER1 + BORDER2) + TITLE };
}

void gui_draw_all(void) {
    struct graphic g;
    graphic_from_fb(&g);
    for (size_t i = 0; i < g_gui.draw_list.size / sizeof(struct window*); i++) {
        struct window* w = arraylist_at(&g_gui.draw_list, struct window*, i);
        graphic_set_offset(&g, &w->rect);
        g.bg_color = w->bg_color;
        draw_window(w, &g);
    }
}

void window_redraw(struct window* w, const struct rect* rt) {
    struct graphic g;
    graphic_from_fb(&g);
    graphic_set_offset(&g, &w->rect);
    graphic_set_clipping(&g, rt);
    g.bg_color = w->bg_color;
    draw_window(w, &g);
}
