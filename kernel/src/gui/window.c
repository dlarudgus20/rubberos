#include <freec/stdlib.h>
#include <freec/assert.h>

#include "gui/gui.h"
#include "gui/window.h"
#include "gui/evt.h"
#include "drivers/framebuffer.h"
#include "memory.h"

struct winman g_winman;

static void draw_mouse(struct graphic* g, int x, int y);

void gui_init(void) {
    intrlock_init(&g_winman.lock);
    SLAB_INIT(&g_winman.slab_window, struct window);
    g_winman.painting = false;

    linkedlist_init(&g_winman.window_list);
    g_winman.focused = NULL;

    const struct fb_info* fi = fb_info_get();
    g_winman.scr_size.width = fi->width;
    g_winman.scr_size.height = fi->height;
    g_winman.mouse_pos.x = fi->width / 2;
    g_winman.mouse_pos.y = fi->height / 2;

    g_winman.global_invalidated = (struct rect){ 0, 0, fi->width, fi->height };
    g_winman.bg_color = 0x001f00;
}

static void invalidate_window_all(struct window* w) {
    w->win_invalidated = (struct rect){ 0, 0, w->scr_rect.width, w->scr_rect.height };
}

struct window* window_new(void) {
    intrlock_acquire(&g_winman.lock);

    struct window* w = slab_alloc(&g_winman.slab_window);
    w->title = "New Window";
    w->scr_rect = (struct rect){ 120, 120, 640, 480 };
    w->bg_color = 0xffffff;
    w->proc = NULL;
    invalidate_window_all(w);

    linkedlist_push_back(&g_winman.window_list, &w->link);

    intrlock_release(&g_winman.lock);
    return w;
}

#define BORDER1 1
#define BORDER2 2
#define TITLE_X_MARGIN 20
#define TITLE_Y_MARGIN 2
#define TITLE_HEIGHT (16 + 2 * TITLE_Y_MARGIN)

#define CLIENT_X0 (BORDER1 + BORDER2)
#define CLIENT_Y0 (BORDER1 + BORDER2 + TITLE_HEIGHT)

struct size window_size_for_client(int width, int height) {
    return (struct size){ width + 2 * (BORDER1 + BORDER2), height + 2 * (BORDER1 + BORDER2) + TITLE_HEIGHT };
}

struct rect client_rect_for_window(int width, int height) {
    struct rect client = { CLIENT_X0, CLIENT_Y0, width - 2 * CLIENT_X0, height - CLIENT_X0 - CLIENT_Y0 };
    if (client.width < 0) client.width = 0;
    if (client.height < 0) client.height = 0;
    return client;
}

static struct rect scr_to_win(const struct window* w, const struct rect* rt) {
    return (struct rect){ rt->x - w->scr_rect.x, rt->y - w->scr_rect.y, rt->width, rt->height };
}

static struct rect win_to_scr(const struct window* w, const struct rect* rt) {
    return (struct rect){ rt->x + w->scr_rect.x, rt->y + w->scr_rect.y, rt->width, rt->height };
}

static void draw_window(struct window* w, struct graphic* g) {
    graphic_set_offset(g, &w->scr_rect);
    graphic_set_clipping(g, &w->win_invalidated);
    g->bg_color = w->bg_color;

    graphic_draw_rect(g, 0, 0, w->scr_rect.width, w->scr_rect.height, BORDER1, 0x2f2f2f);
    graphic_draw_rect(g, BORDER1, BORDER1, w->scr_rect.width - 2 * BORDER1, w->scr_rect.height - 2 * BORDER1, BORDER2, 0x3f3f3f);

    const int border = BORDER1 + BORDER2;
    const int inborder_width = w->scr_rect.width - 2 * border;
    const int inborder_height = w->scr_rect.height - 2 * border;
    const int title_height = inborder_height > TITLE_HEIGHT ? TITLE_HEIGHT : inborder_height;

    graphic_fill_rect(g, border, border, inborder_width, title_height, 0x5f5f5f);

    struct rect title_str = { border + TITLE_X_MARGIN, border + 2, inborder_width - TITLE_X_MARGIN, 16 };
    graphic_draw_string(g, &title_str, w->title, 0x000000, false);

    struct rect client = { border, border + TITLE_HEIGHT, inborder_width, inborder_height - TITLE_HEIGHT };
    graphic_fill_rect(g, client.x, client.y, client.width, client.height, g->bg_color);

    if (w->proc) {
        struct rect client_inv = rect_intersect(&w->win_invalidated, &client);
        struct rect client_scr = win_to_scr(w, &client);
        graphic_set_offset(g, &client_scr);
        graphic_set_clipping(g, &(struct rect){
            client_inv.x - client.x, client_inv.y - client.y,
            client_inv.width, client_inv.height
        });
        w->proc(w, WM_PAINT, g);
    }

    w->win_invalidated = (struct rect){ 0 };
}

void gui_draw_all(void) {
    struct singlylist draw_list;
    singlylist_init(&draw_list);

    // lock
    intrlock_acquire(&g_winman.lock);

    struct rect gi = g_winman.global_invalidated;
    struct size scr = g_winman.scr_size;
    struct point mouse = g_winman.mouse_pos;
    color_t bg = g_winman.bg_color;

    assert(!g_winman.painting);
    g_winman.painting = true;
    linkedlist_foreach_backward(ptr, &g_winman.window_list) {
        struct window* w = container_of(ptr, struct window, link);
        singlylist_push_front(&draw_list, &w->draw_link);
    }

    intrlock_release(&g_winman.lock);

    // draw without lock
    struct graphic g;
    graphic_from_fb(&g);

    graphic_set_clipping(&g, &gi);
    graphic_fill_rect(&g, 0, 0, scr.width, scr.height, bg);

    singlylist_foreach(ptr, &draw_list) {
        struct window* w = container_of(ptr, struct window, draw_link);
        draw_window(w, &g);
    }

    graphic_set_offset(&g, &(struct rect){ 0, 0, scr.width, scr.height });
    draw_mouse(&g, mouse.x, mouse.y);

    // lock
    intrlock_acquire(&g_winman.lock);

    g_winman.global_invalidated = (struct rect){ 0 };
    g_winman.painting = false;

    intrlock_release(&g_winman.lock);
}

static void invalidate_global(const struct rect* rt) {
    linkedlist_foreach(ptr, &g_winman.window_list) {
        struct window* w = container_of(ptr, struct window, link);
        struct rect inv = rect_intersect(&w->scr_rect, rt);
        struct rect scr = scr_to_win(w, &inv);
        w->win_invalidated = rect_union(&w->win_invalidated, &scr);
    }
    g_winman.global_invalidated = rect_union(&g_winman.global_invalidated, rt);
}

static void invalidate_client(struct window* w, const struct rect* rt) {
    if (rt) {
        struct rect client = *rt;
        client.x += CLIENT_X0;
        client.y += CLIENT_Y0;
        w->win_invalidated = rect_union(&w->win_invalidated, &client);
    } else {
        invalidate_window_all(w);
    }
}

void window_invalidate(struct window* w, const struct rect* rt) {
    intrlock_acquire(&g_winman.lock);
    invalidate_client(w, rt);
    intrlock_release(&g_winman.lock);
}

void window_redraw(struct window* w, const struct rect* rt) {
    intrlock_acquire(&g_winman.lock);

    invalidate_client(w, rt);

    struct graphic g;
    graphic_from_fb(&g);
    draw_window(w, &g);

    intrlock_release(&g_winman.lock);
}

void window_set_focus(struct window* w) {
    intrlock_acquire(&g_winman.lock);
    g_winman.focused = w;
    intrlock_release(&g_winman.lock);
}

void window_sendmsg_focused(enum window_message msg, void* param) {
    struct window* focused;

    intrlock_acquire(&g_winman.lock);
    focused = g_winman.focused;
    intrlock_release(&g_winman.lock);

    if (focused && focused->proc) {
        focused->proc(focused, msg, param);
    }
}

static void on_nc_mouse(struct window* w, struct gui_mouse_event* evt) {
    // TODO: process non-client mouse event
}

void window_sendmsg_mouse(int x, int y, struct gui_mouse_event evt) {
    struct window* target = NULL;

    // lock
    intrlock_acquire(&g_winman.lock);

    linkedlist_foreach_backward(ptr, &g_winman.window_list) {
        struct window* w = container_of(ptr, struct window, link);
        if (rect_contains(&w->scr_rect, x, y)) {
            target = w;
            break;
        }
    }

    if (target) {
        struct rect client = client_rect_for_window(target->scr_rect.width, target->scr_rect.height);
        evt.x -= target->scr_rect.x;
        evt.y -= target->scr_rect.y;
        if (rect_contains(&client, evt.x, evt.y)) {
            evt.x -= client.x;
            evt.y -= client.y;
        } else {
            on_nc_mouse(target, &evt);
            target = NULL;
        }
    }

    intrlock_release(&g_winman.lock);

    // sendmsg without lock
    if (target && target->proc) {
        target->proc(target, WM_MOUSE, &evt);
    }
}

#define MOUSE_WIDTH 13
#define MOUSE_HEIGHT 19

static void draw_mouse(struct graphic* g, int x, int y) {
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

struct point gui_mouse_move(int dx, int dy) {
    intrlock_acquire(&g_winman.lock);

    struct rect old = { g_winman.mouse_pos.x, g_winman.mouse_pos.y, MOUSE_WIDTH, MOUSE_HEIGHT };

    g_winman.mouse_pos.x += dx;
    if (g_winman.mouse_pos.x < 0) {
        g_winman.mouse_pos.x = 0;
    } else if (g_winman.mouse_pos.x >= g_winman.scr_size.width) {
        g_winman.mouse_pos.x = g_winman.scr_size.width;
    }

    g_winman.mouse_pos.y += dy;
    if (g_winman.mouse_pos.y < 0) {
        g_winman.mouse_pos.y = 0;
    } else if (g_winman.mouse_pos.y >= g_winman.scr_size.height) {
        g_winman.mouse_pos.y = g_winman.scr_size.height;
    }

    struct rect new = { g_winman.mouse_pos.x, g_winman.mouse_pos.y, MOUSE_WIDTH, MOUSE_HEIGHT };
    struct rect inv = rect_union(&old, &new);
    invalidate_global(&inv);

    struct point pos = g_winman.mouse_pos;
    intrlock_release(&g_winman.lock);
    return pos;
}
