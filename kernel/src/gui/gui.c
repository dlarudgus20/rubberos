#include <freec/stdlib.h>
#include <freec/assert.h>
#include <collections/arraylist.h>
#include <slab/slab.h>

#include "gui/gui.h"
#include "gui/graphic.h"
#include "drivers/framebuffer.h"
#include "memory.h"
#include "spinlock.h"

struct gui {
    struct intrlock lock;
    struct slab_allocator slab_window;
    struct arraylist draw_list;

    struct size total_size;
    struct point mouse;

    struct rect global_invalidated;
    color_t bg_color;
};

static struct gui g_gui;

void gui_init(void) {
    intrlock_init(&g_gui.lock);
    SLAB_INIT(&g_gui.slab_window, struct window);
    arraylist_init(&g_gui.draw_list, 0, &g_arraylist_allocator);

    const struct fb_info* fi = fb_info_get();
    g_gui.total_size.width = fi->width;
    g_gui.total_size.height = fi->height;
    g_gui.mouse.x = fi->width / 2;
    g_gui.mouse.y = fi->height / 2;

    g_gui.global_invalidated = (struct rect){ .x = 0, .y = 0, .width = fi->width, .height = fi->height };
    g_gui.bg_color = 0x001f00;
}

static void invalidate_all(struct window* w) {
    w->invalidated = (struct rect){ .x = 0, .y = 0, .width = w->rect.width, .height = w->rect.height };
}

struct window* window_new(void) {
    intrlock_acquire(&g_gui.lock);

    struct window* w = slab_alloc(&g_gui.slab_window);
    w->title = "New Window";
    w->rect = (struct rect){ .x = 120, .y = 120, .width = 640, .height = 480 };
    w->bg_color = 0xffffff;
    w->proc = NULL;
    invalidate_all(w);

    *(struct window**)arraylist_push_back(&g_gui.draw_list, sizeof(struct window*)) = w;

    intrlock_release(&g_gui.lock);
    return w;
}

#define BORDER1 1
#define BORDER2 2
#define TITLE 20

#define CLIENT_X0 (BORDER1 + BORDER2)
#define CLIENT_Y0 (BORDER1 + BORDER2 + TITLE)

static void draw_window(struct window* w, struct graphic* g) {
    graphic_set_offset(g, &w->rect);
    graphic_set_clipping(g, &w->invalidated);
    g->bg_color = w->bg_color;

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
        struct rect client_inv = rect_intersect(&w->invalidated, &client);
        graphic_set_offset(g, &(struct rect){
            client.x + w->rect.x, client.y + w->rect.y, client.width, client.height
        });
        graphic_set_clipping(g, &(struct rect){
            client_inv.x - client.x, client_inv.y - client.y,
            client_inv.width, client_inv.height
        });
        w->proc(w, WM_PAINT, g);
    }

    w->invalidated = (struct rect){ 0 };
}

struct size window_size_for_client(int width, int height) {
    return (struct size){ width + 2 * (BORDER1 + BORDER2), height + 2 * (BORDER1 + BORDER2) + TITLE };
}

#define MOUSE_WIDTH 13
#define MOUSE_HEIGHT 19

static void draw_mouse(struct graphic* g) {
    static const char block[] =
        "*............"
        "**..........."
        "*-*.........."
        "*o-*........."
        "*oo-*........"
        "*ooo-*......."
        "*oooo-*......"
        "*ooooo-*....."
        "*oooooo-*...."
        "*ooooooo-*..."
        "*oooooooo-*.."
        "*ooooooooo-*."
        "*oooooo******"
        "*ooo*-o*....."
        "*oo*.*oo*...."
        "*-*..*-o*...."
        "**....*oo*..."
        "*.....*--*..."
        ".......**....";

    static_assert(sizeof(block) == MOUSE_HEIGHT * MOUSE_WIDTH + 1, "mouse block size does not match");

    const int x = g_gui.mouse.x;
    const int y = g_gui.mouse.y;

    for (int yi = 0; yi < MOUSE_HEIGHT; yi++) {
        for (int xi = 0; xi < MOUSE_WIDTH; xi++) {
            color_t color;
            switch (block[yi * MOUSE_WIDTH + xi]) {
                case '*': color = 0x000000; break;
                case '@': color = 0x404040; break;
                case '/': color = 0x808080; break;
                case '-': color = 0xc0c0c0; break;
                case 'o': color = 0xffffff; break;
                default: continue;
            }
            graphic_draw_pixel(g, x + xi, y + yi, color);
        }
    }
}

static struct rect scr_to_win(const struct window* w, const struct rect* rt) {
    return (struct rect){
        .x = rt->x - w->rect.x,
        .y = rt->y - w->rect.y,
        .width = rt->width,
        .height = rt->height
    };
}

static void global_invalidate(const struct rect* rt) {
    arraylist_foreach(struct window**, pw, &g_gui.draw_list) {
        struct window* w = *pw;
        struct rect inter = rect_intersect(&w->rect, rt);
        struct rect scr = scr_to_win(w, &inter);
        w->invalidated = rect_union(&w->invalidated, &scr);
    }
    g_gui.global_invalidated = rect_union(&g_gui.global_invalidated, rt);
}

void gui_mouse_move(int dx, int dy) {
    intrlock_acquire(&g_gui.lock);

    struct rect old = { g_gui.mouse.x, g_gui.mouse.y, MOUSE_WIDTH, MOUSE_HEIGHT };

    g_gui.mouse.x += dx;
    if (g_gui.mouse.x < 0) {
        g_gui.mouse.x = 0;
    } else if (g_gui.mouse.x >= g_gui.total_size.width) {
        g_gui.mouse.x = g_gui.total_size.width;
    }

    g_gui.mouse.y += dy;
    if (g_gui.mouse.y < 0) {
        g_gui.mouse.y = 0;
    } else if (g_gui.mouse.y >= g_gui.total_size.height) {
        g_gui.mouse.y = g_gui.total_size.height;
    }

    struct rect new = { g_gui.mouse.x, g_gui.mouse.y, MOUSE_WIDTH, MOUSE_HEIGHT };
    struct rect inv = rect_union(&old, &new);
    global_invalidate(&inv);

    intrlock_release(&g_gui.lock);
}

void gui_draw_all(void) {
    intrlock_acquire(&g_gui.lock);

    struct graphic g;
    graphic_from_fb(&g);

    graphic_set_clipping(&g, &g_gui.global_invalidated);
    graphic_fill_rect(&g, 0, 0, g_gui.total_size.width, g_gui.total_size.height, g_gui.bg_color);

    arraylist_foreach(struct window**, pw, &g_gui.draw_list) {
        draw_window(*pw, &g);
    }

    graphic_set_offset(&g, &(struct rect){ 0, 0, g_gui.total_size.width, g_gui.total_size.height });
    draw_mouse(&g);

    g_gui.global_invalidated = (struct rect){ 0 };

    intrlock_release(&g_gui.lock);
}

static void invalidate_client(struct window* w, const struct rect* rt) {
    if (rt) {
        struct rect client = *rt;
        client.x += CLIENT_X0;
        client.y += CLIENT_Y0;
        w->invalidated = rect_union(&w->invalidated, &client);
    } else {
        invalidate_all(w);
    }
}

void window_invalidate(struct window* w, const struct rect* rt) {
    intrlock_acquire(&g_gui.lock);
    invalidate_client(w, rt);
    intrlock_release(&g_gui.lock);
}

void window_redraw(struct window* w, const struct rect* rt) {
    intrlock_acquire(&g_gui.lock);

    invalidate_client(w, rt);

    struct graphic g;
    graphic_from_fb(&g);
    draw_window(w, &g);

    intrlock_release(&g_gui.lock);
}
