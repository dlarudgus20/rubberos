#pragma once

#include "memory.h"
#include "drivers/framebuffer.h"

#define BOOTINFO_CMD_MAXLEN 256
#define BOOTINFO_MMAP_MAXLEN 128

struct bootinfo {
    const char* cmd;
    const struct mmap* mmap;
    struct fb_info fb_info;
};

const struct bootinfo* bootinfo_get(void);
