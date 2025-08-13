#pragma once

#include <stdint.h>
#include "gui/shapes.h"

typedef uint32_t color_t;

struct graphic {
    color_t* framebuffer;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;

    struct rect offset;
    struct rect clipping;
    color_t bg_color;
};

void graphic_from_fb(struct graphic* g);

void graphic_set_offset(struct graphic* g, const struct rect* rt);
void graphic_set_clipping(struct graphic* g, const struct rect* rt);

void graphic_draw_pixel(struct graphic* g, int x, int y, color_t color);
void graphic_fill_rect(struct graphic* g, int x, int y, int width, int height, color_t color);
void graphic_draw_rect(struct graphic* g, int x, int y, int width, int height, int thickness, color_t color);
void graphic_draw_char(struct graphic* g, int x, int y, char c, color_t color);
void graphic_draw_string(struct graphic* g, const struct rect* rt, const char* str, color_t color, bool wrap);

void graphic_bitblt(struct graphic* g, int x, int y, int cx, int cy, int x0, int y0);
