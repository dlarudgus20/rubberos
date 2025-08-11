#include "drivers/framebuffer.h"
#include "memory.h"
#include "boot.h"

static volatile uint32_t* g_fb;

void fb_init(void) {
    const struct fb_info* fi = &bootinfo_get()->fb_info;

    g_fb = mmio_alloc_mapping(fi->addr, fi->addr + fi->pitch * fi->height);

    for (uint32_t y = 0; y < fi->height; y++) {
        for (uint32_t x = 0; x < fi->width; x++) {
            uint32_t color = x ^ y;
            if (y < 10 || y >= fi->height - 10 || x < 10 || x >= fi->width - 10) {
                color = 0x0000ff00;
            }
            g_fb[x + y * fi->pitch / sizeof(*g_fb)] = color;
        }
    }
}
