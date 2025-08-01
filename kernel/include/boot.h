#pragma once

#include "memory.h"
#include "drivers/framebuffer.h"

struct bootinfo {
    const char* cmd;
    const struct mmap* mmap;
    struct fb_info fb_info;
};

const struct bootinfo* bootinfo_get(void);
