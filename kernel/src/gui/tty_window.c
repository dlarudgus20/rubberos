#include <freec/stdlib.h>
#include <freec/string.h>
#include <freec/assert.h>

#include "gui/tty_window.h"
#include "gui/window.h"
#include "gui/evt.h"

#include "tty.h"
#include "drivers/hid.h"

#define CW 8
#define CH 16

struct lines {
    uint16_t begin;
    uint16_t end;
};

static struct lines lines_union(struct lines a, struct lines b) {
    if (b.begin == b.end) {
        return a;
    } else if (a.begin == a.end) {
        return b;
    }
    return (struct lines){ MIN(a.begin, b.begin), MAX(a.end, b.end) };
}

static void scroll(struct tty_window* tw) {
    memmove(tw->buffer, tw->buffer + TTYW_WIDTH, TTYW_WIDTH * (TTYW_HEIGHT - 1));
    memset(tw->buffer + TTYW_WIDTH * (TTYW_HEIGHT - 1), ' ', TTYW_WIDTH);
    if (tw->cursor_y > 0) {
        tw->cursor_y -= 1;
    } else {
        tw->cursor_x = 0;
    }
    window_invalidate(tw->window, NULL);
}

static struct lines newline(struct tty_window* tw) {
    tw->cursor_x = 0;
    if (++tw->cursor_y >= TTYW_HEIGHT) {
        scroll(tw);
        return (struct lines){ 0, TTYW_HEIGHT };
    } else {
        return (struct lines){ tw->cursor_y - 1, tw->cursor_y + 1 };
    }
}

static struct lines write_one(struct tty_window* tw, char ch) {
    if (ch == '\n') {
        return newline(tw);
    } else {
        tw->buffer[tw->cursor_y * TTYW_WIDTH + tw->cursor_x] = ch;
        if (++tw->cursor_x >= TTYW_WIDTH) {
            return newline(tw);
        } else {
            return (struct lines){ tw->cursor_y, tw->cursor_y + 1 };
        }
    }
}

static void invalidate_lines(struct tty_window* tw, struct lines lines) {
    window_invalidate(tw->window, &(struct rect){
        .x = 0, .y = lines.begin * CH, .width = TTYW_WIDTH * CW, .height = (lines.end - lines.begin) * CH
    });
}

static void write(struct tty_device* device, const char* str) {
    struct tty_window* tw = container_of(device, struct tty_window, device);
    struct lines lines = { 0 };
    while (*str != 0) {
        lines = lines_union(lines, write_one(tw, *str++));
    }
    invalidate_lines(tw, lines);
}

static void flush(struct tty_device* device) {
    struct tty_window* tw = container_of(device, struct tty_window, device);
    window_redraw(tw->window, NULL);
}

#define CURSOR_THICKNESS 4
#define BOX_RADIUS 5

static void proc(struct window* w, enum window_message msg, void* param) {
    struct tty_window* tw = w->data;

    static struct point rp = { .x = -10, .y = -10 };

    switch (msg) {
        case WM_PAINT: {
            struct graphic* g = param;

            int cy = MAX(g->clipping.y / CH, 0);
            int cx = MAX(g->clipping.x / CW, 0);
            int cheight = MIN((g->clipping.y + g->clipping.height + CH - 1) / CH, TTYW_HEIGHT);
            int cwidth = MIN((g->clipping.x + g->clipping.width + CW - 1) / CW, TTYW_WIDTH);
            for (int y = cy; y < cheight; y++) {
                for (int x = cx; x < cwidth; x++) {
                    graphic_draw_char(g, x * CW, y * CH, tw->buffer[y * TTYW_WIDTH + x], 0x000000);
                }
            }

            graphic_fill_rect(g, tw->cursor_x * CW, (tw->cursor_y + 1) * CH - CURSOR_THICKNESS, CW, CURSOR_THICKNESS, 0x1f1f1f);
            graphic_fill_rect(g, rp.x - BOX_RADIUS, rp.y - BOX_RADIUS, BOX_RADIUS, BOX_RADIUS, 0xff0000);
            break;
        }
        case WM_CHAR: {
            struct hid_char c = *(struct hid_char*)param;
            struct lines lines = write_one(tw, c.ch);
            invalidate_lines(tw, lines);
            break;
        }
        case WM_MOUSE: {
            struct gui_mouse_event evt = *(struct gui_mouse_event*)param;
            if (evt.left) {
                rp.x = evt.x;
                rp.y = evt.y;
                window_invalidate(w, &(struct rect){ rp.x - BOX_RADIUS, rp.y - BOX_RADIUS, BOX_RADIUS, BOX_RADIUS });
            }
            break;
        }
    }
}

void tty_window_init(struct tty_window* tw, struct tty* tty) {
    tw->window = window_new();
    assert(tw->window);

    struct size size = window_size_for_client(TTYW_WIDTH * CW, TTYW_HEIGHT * CH);
    tw->window->scr_rect.width = size.width;
    tw->window->scr_rect.height = size.height;
    tw->window->title = "TTY";
    tw->window->data = tw;
    tw->window->proc = proc;

    memset(tw->buffer, ' ', sizeof(tw->buffer));

    tw->device.write = write;
    tw->device.flush = flush;
    tty_register_device(tty, &tw->device);

    window_set_focus(tw->window);
}
