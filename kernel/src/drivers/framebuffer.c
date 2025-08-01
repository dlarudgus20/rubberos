#include "drivers/framebuffer.h"
#include "boot.h"

void fb_init(void) {
    const struct fb_info* fi = &bootinfo_get()->fb_info;
    volatile uint32_t* fb = fi->addr;
    for (uint32_t y = 0; y < fi->height; y++) {
        for (uint32_t x = 0; x < fi->width; x++) {
            uint32_t color = x ^ y;
            if (y < 10 || y >= fi->height - 10 || x < 10 || x >= fi->width - 10) {
                color = 0x0000ff00;
            }
            fb[x + y * fi->width] = color;
        }
    }
}
