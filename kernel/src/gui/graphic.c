#include "gui/graphic.h"
#include "drivers/framebuffer.h"

extern unsigned char g_ascii_font[];

void graphic_from_fb(struct graphic* g) {
    const struct fb_info* fi = fb_info_get();
    g->framebuffer = fb_buffer_get();
    g->pitch = fi->pitch / sizeof(*g->framebuffer);
    g->width = fi->width;
    g->height = fi->height;
    g->offset = (struct rect){ .x = 0, .y = 0, .width = fi->width, .height = fi->height };
    g->clipping = (struct rect){ .x = 0, .y = 0, .width = fi->width, .height = fi->height };
    g->bg_color = 0;
}

void graphic_set_offset(struct graphic* g, const struct rect* rt) {
    g->offset = rect_intersect(rt, &(struct rect){ .x = 0, .y = 0, .width = g->width, .height = g->height });
    g->clipping = (struct rect){ .x = 0, .y = 0, .width = g->offset.width, .height = g->offset.height };
}

void graphic_set_clipping(struct graphic* g, const struct rect* rt) {
    g->clipping = rect_intersect(rt, &(struct rect){ .x = 0, .y = 0, .width = g->offset.width, .height = g->offset.height });
}

void graphic_draw_pixel(struct graphic* g, int x, int y, uint32_t color) {
    if (rect_is_in(&g->clipping, x, y)) {
        g->framebuffer[(x + g->offset.x) + (y + g->offset.y) * g->pitch] = color;
    }
}

void graphic_fill_rect(struct graphic* g, int x, int y, int width, int height, uint32_t color) {
    struct rect r = rect_intersect(&g->clipping, &(struct rect){
        .x = x, .y = y, .width = width, .height = height });
    for (int yi = 0; yi < r.height; yi++) {
        for (int xi = 0; xi < r.width; xi++) {
            g->framebuffer[(g->offset.x + r.x + xi) + (g->offset.y + r.y + yi) * g->pitch] = color;
        }
    }
}

void graphic_draw_rect(struct graphic* g, int x, int y, int width, int height, int thickness, uint32_t color) {
    graphic_fill_rect(g, x, y, width, thickness, color);
    graphic_fill_rect(g, x, y + height - thickness, width, thickness, color);
    graphic_fill_rect(g, x, y + thickness, thickness, height - 2 * thickness, color);
    graphic_fill_rect(g, x + width - thickness, y + thickness, thickness, height - 2 * thickness, color);
}

void graphic_draw_char(struct graphic* g, int x, int y, char c, uint32_t color) {
    unsigned char* font = g_ascii_font + (unsigned char)c * 16;
    for (int yi = 0; yi < 16; yi++) {
        if (font[yi] & 0x80) {
            graphic_draw_pixel(g, x + 0, y + yi, color);
        }
        if (font[yi] & 0x40) {
            graphic_draw_pixel(g, x + 1, y + yi, color);
        }
        if (font[yi] & 0x20) {
            graphic_draw_pixel(g, x + 2, y + yi, color);
        }
        if (font[yi] & 0x10) {
            graphic_draw_pixel(g, x + 3, y + yi, color);
        }
        if (font[yi] & 0x08) {
            graphic_draw_pixel(g, x + 4, y + yi, color);
        }
        if (font[yi] & 0x04) {
            graphic_draw_pixel(g, x + 5, y + yi, color);
        }
        if (font[yi] & 0x02) {
            graphic_draw_pixel(g, x + 6, y + yi, color);
        }
        if (font[yi] & 0x01) {
            graphic_draw_pixel(g, x + 7, y + yi, color);
        }
    }
}

void graphic_draw_string(struct graphic* g, const struct rect* rt, const char* str, uint32_t color, bool wrap) {
    struct rect old_clip = g->clipping;
    struct rect c = rect_intersect(&old_clip, rt);
    g->clipping = c;

    int x = rt->x;
    int y = rt->y;

    for (const char* p = str; *p != 0; p++) {
        if (*p == '\n') {
            x = rt->x;
            y += 16;
            continue;
        }

        if (!wrap && x >= c.x + c.width) {
            break;
        }
        if (wrap && x + 8 > c.x + c.width) {
            x = rt->x;
            y += 16;
        }

        graphic_draw_char(g, x, y, *p, color);
        x += 8;
    }

    g->clipping = old_clip;
}

void graphic_bitblt(struct graphic* g, int x, int y, int cx, int cy, int x0, int y0) {
    struct rect rd = rect_intersect(&g->clipping, &(struct rect){
        .x = x, .y = y, .width = cx, .height = cy });

    int dx = rd.x - x;
    int dy = rd.y - y;

    int leftpad = x0 > 0 ? 0 : -x0;
    int rightpad = x0 + cx < g->offset.width ? 0 : (x0 + cx - g->offset.width);

    for (int yi = dy; rd.y + yi < rd.height; yi++) {
        for (int xi = dx; rd.x + xi < rd.width; xi++) {
            color_t* d = g->framebuffer + (g->offset.x + rd.x + xi) + (g->offset.y + rd.y + yi) * g->pitch;
            if (xi < leftpad || cx - xi <= rightpad) {
                *d = g->bg_color;
            } else {
                int sx = g->offset.x + x0 + xi;
                int sy = g->offset.y + y0 + yi;
                *d = g->framebuffer[sx + sy * g->pitch];
            }
        }
    }
}
