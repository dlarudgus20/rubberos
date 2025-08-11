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
void fb_write_char(int x, int y, char c, uint32_t color);
void fb_write_string(int x, int y, const char* str, uint32_t color, bool wrap);
