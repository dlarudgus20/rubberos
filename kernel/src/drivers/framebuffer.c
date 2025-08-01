#include "framebuffer.h"
#include "../arch/x86_64/boot/boot.h"

void fb_init(void) {
    const struct bootinfo* bi = bootinfo_get();
    for (int y = 0; y < bi->height; y++) {
        for (int x = 0; x < bi->width; x++) {
            int color = x ^ y;
            if (y < 10 || y >= bi->height - 10 || x < 10 || x >= bi->width - 10) {
                color = 0x0000ff00;
            }
            bi->video[x + y * bi->width] = color;
        }
    }
}
