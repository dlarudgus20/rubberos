#pragma once

#include <stdint.h>
#include <stdbool.h>

struct fb_info {
    uint32_t addr;
    uint32_t pitch;
    int32_t width;
    int32_t height;
    uint8_t bpp;
    uint8_t type;
};

void fb_init(void);
const struct fb_info* fb_info_get(void);
uint32_t* fb_buffer_get(void);
