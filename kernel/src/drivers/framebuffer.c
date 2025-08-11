#include "drivers/framebuffer.h"
#include "memory.h"
#include "boot.h"

extern unsigned char g_ascii_font[];

static volatile uint32_t* g_fb;

static const struct fb_info* fb_info_get() {
    return &bootinfo_get()->fb_info;
}

void fb_init(void) {
    const struct fb_info* fi = fb_info_get();

    g_fb = mmio_alloc_mapping(fi->addr, fi->addr + fi->pitch * fi->height);

    char buf[256];
    for (size_t i = 0; i < 255; i++) buf[i] = i + 1;
    buf[sizeof(buf) - 1] = 0;
    fb_write_string(0, 0, buf, 0x00ff00, true);
}

void fb_write_char(int x, int y, char c, uint32_t color) {
    unsigned char* font = g_ascii_font + (unsigned char)c * 16;
    const struct fb_info* fi = fb_info_get();
    for (int yi = 0; yi < 16; yi++) {
        if (y + yi < 0 || y + yi >= fi->height) {
            continue;
        }
        for (int xi = 0; xi < 8; xi++) {
            if (x + xi < 0 || x + xi >= fi->width) {
                continue;
            }
            if (font[yi] & (1 << (7 - xi))) {
                g_fb[(x + xi) + (y + yi) * fi->pitch / sizeof(*g_fb)] = color;
            }
        }
    }
}

void fb_write_string(int x, int y, const char* str, uint32_t color, bool wrap) {
    const struct fb_info* fi = fb_info_get();
    int xi = 0, yi = 0;
    for (; *str != 0; str++) {
        fb_write_char(x + xi, y + yi, *str, color);
        xi += 8;
        if (!wrap && x + xi >= fi->width) {
            break;
        } else if (wrap && x + xi + 8 > fi->width) {
            xi = 0;
            yi += 16;
            if (y + yi >= fi->height) {
                break;
            }
        }
    }
}
