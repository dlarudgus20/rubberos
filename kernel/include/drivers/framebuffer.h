#pragma once

#include <stdint.h>

struct fb_info {
    uint32_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t type;
};

void fb_init(void);
