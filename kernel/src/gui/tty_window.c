#include <freec/stdlib.h>
#include <freec/string.h>
#include <freec/assert.h>

#include "gui/tty_window.h"
#include "gui/gui.h"
#include "tty.h"

struct lines {
    uint16_t begin;
    uint16_t end;
};

static void scroll(struct tty_window* tw) {
    memmove(tw->buffer, tw->buffer + TTYW_WIDTH, TTYW_WIDTH * (TTYW_HEIGHT - 1));
    memset(tw->buffer + TTYW_WIDTH * (TTYW_HEIGHT - 1), ' ', TTYW_WIDTH);
    if (tw->cursor_y > 0) {
        tw->cursor_y -= 1;
    } else {
        tw->cursor_x = 0;
    }
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

static void write(struct tty_device* device, const char* str) {
    struct tty_window* tw = container_of(device, struct tty_window, device);
    while (*str != 0) {
        write_one(tw, *str++);
    }
}

static void flush(struct tty_device* device) {
    struct tty_window* tw = container_of(device, struct tty_window, device);
    window_redraw(tw->window, NULL);
}

#define CURSOR_THICKNESS 4

static void proc(struct window* w, enum window_message msg, void* param) {
    if (msg == WM_PAINT) {
        struct graphic* g = param;
        struct tty_window* tw = w->data;

        int cy = MAX(g->clipping.y / 16, 0);
        int cx = MAX(g->clipping.x / 8, 0);
        int cheight = MIN((g->clipping.y + g->clipping.height + 15) / 16, TTYW_HEIGHT);
        int cwidth = MIN((g->clipping.x + g->clipping.width + 7) / 8, TTYW_WIDTH);
        for (int y = cy; y < cheight; y++) {
            for (int x = cx; x < cwidth; x++) {
                graphic_draw_char(g, x * 8, y * 16, tw->buffer[y * TTYW_WIDTH + x], 0x000000);
            }
        }

        graphic_fill_rect(g, tw->cursor_x * 8, (tw->cursor_y + 1) * 16 - CURSOR_THICKNESS, 8, CURSOR_THICKNESS, 0x1f1f1f);
    }
}

void tty_window_init(struct tty_window* tw, struct tty* tty) {
    tw->window = window_new();
    assert(tw->window);

    struct size size = window_size_for_client(TTYW_WIDTH * 8, TTYW_HEIGHT * 16);
    tw->window->rect.width = size.width;
    tw->window->rect.height = size.height;
    tw->window->title = "TTY";
    tw->window->data = tw;
    tw->window->proc = proc;

    memset(tw->buffer, ' ', sizeof(tw->buffer));

    tw->device.write = write;
    tw->device.flush = flush;
    tty_register_device(tty, &tw->device);
}
