#include <stddef.h>
#include <freec/stdlib.h>
#include <freec/string.h>
#include <freec/assert.h>
#include <slab/slab.h>

#include "gui/graphic.h"
#include "drivers/framebuffer.h"
#include "memory.h"

extern unsigned char g_ascii_font[];

static struct slab_allocator g_slab_rects;

void graphic_init(void) {
    SLAB_INIT(&g_slab_rects, struct rect);
}

void graphic_from_fb(struct graphic* g) {
    const struct fb_info* fi = fb_info_get();
    g->framebuffer = fb_buffer_get();
    g->pitch = fi->pitch / sizeof(*g->framebuffer);
    g->width = fi->width;
    g->height = fi->height;
    g->offset = (struct rect){ 0, 0, fi->width, fi->height };
    g->clipping = (struct rect){ 0, 0, fi->width, fi->height };
}

void graphic_create_memory(struct graphic* g) {
    const struct fb_info* fi = fb_info_get();
    struct slice mem = dynmem_alloc(fi->width * fi->height * sizeof(color_t));
    assert(mem.ptr);

    g->framebuffer = mem.ptr;
    g->pitch = fi->width;
    g->width = fi->width;
    g->height = fi->height;
    g->offset = (struct rect){ 0, 0, fi->width, fi->height };
    g->clipping = (struct rect){ 0, 0, fi->width, fi->height };
}

void graphic_destroy_memory(struct graphic* g) {
    dynmem_dealloc(g->framebuffer, g->pitch * g->height * sizeof(color_t));
}

void graphic_set_offset(struct graphic* g, const struct rect* rt) {
    struct rect base = { 0, 0, g->width, g->height };
    g->offset = rt ? *rt : base;
    graphic_set_clipping(g, NULL);
}

void graphic_set_clipping(struct graphic* g, const struct rect* rt) {
    int ow = MIN(g->offset.width, (int)g->width - g->offset.x);
    int oh = MIN(g->offset.height, (int)g->height - g->offset.y);

    int x = -MIN(0, g->offset.x);
    int y = -MIN(0, g->offset.y);
    int w = MAX(ow - x, 0);
    int h = MAX(oh - y, 0);
    struct rect base = { x, y, w, h };

    g->clipping = rt ? rect_intersect(rt, &base) : base;
}

void graphic_draw_pixel(struct graphic* g, int x, int y, uint32_t color) {
    if (rect_contains(&g->clipping, x, y)) {
        g->framebuffer[(x + g->offset.x) + (y + g->offset.y) * g->pitch] = color;
    }
}

void graphic_fill_rect(struct graphic* g, int x, int y, int width, int height, uint32_t color) {
    struct rect r = rect_intersect(&g->clipping, &(struct rect){ x, y, width, height });
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

void graphic_fill_rect_xor(struct graphic* g, int x, int y, int width, int height, uint32_t color) {
    struct rect r = rect_intersect(&g->clipping, &(struct rect){ x, y, width, height });
    for (int yi = 0; yi < r.height; yi++) {
        for (int xi = 0; xi < r.width; xi++) {
            g->framebuffer[(g->offset.x + r.x + xi) + (g->offset.y + r.y + yi) * g->pitch] ^= color;
        }
    }
}

void graphic_draw_rect_xor(struct graphic* g, int x, int y, int width, int height, int thickness, uint32_t color) {
    graphic_fill_rect_xor(g, x, y, width, thickness, color);
    graphic_fill_rect_xor(g, x, y + height - thickness, width, thickness, color);
    graphic_fill_rect_xor(g, x, y + thickness, thickness, height - 2 * thickness, color);
    graphic_fill_rect_xor(g, x + width - thickness, y + thickness, thickness, height - 2 * thickness, color);
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
    // TODO: seperate wrapping area and clipping area
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

void graphic_bitblt(struct graphic* g, int x, int y, int cx, int cy, struct graphic* g0, int x0, int y0) {
    struct rect dst_rect = { x, y, cx, cy };
    struct rect clipped_dst = rect_intersect(&g->clipping, &dst_rect);

    int skip_x = clipped_dst.x - dst_rect.x;
    int skip_y = clipped_dst.y - dst_rect.y;

    struct rect skipped_src = { x0 + skip_x, y0 + skip_y, cx - skip_x, cy - skip_y };

    int copy_width = MIN(clipped_dst.width, skipped_src.width);
    int copy_height = MIN(clipped_dst.height, skipped_src.height);

    for (int yi = 0; yi < copy_height; yi++) {
        for (int xi = 0; xi < copy_width; xi++) {
            int src_x = g0->offset.x + skipped_src.x + xi;
            int src_y = g0->offset.y + skipped_src.y + yi;
            int dst_x = g->offset.x + clipped_dst.x + xi;
            int dst_y = g->offset.y + clipped_dst.y + yi;

            g->framebuffer[dst_x + dst_y * g->pitch] = g0->framebuffer[src_x + src_y * g0->pitch];
        }
    }
}
