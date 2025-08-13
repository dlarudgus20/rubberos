#include <freec/string.h>
#include <freec/assert.h>

#include "drivers/framebuffer.h"
#include "memory.h"
#include "boot.h"

static uint32_t* g_fb;

void fb_init(void) {
    const struct fb_info* fi = fb_info_get();

    g_fb = (uint32_t*)mmio_alloc_mapping(fi->addr, fi->addr + fi->pitch * fi->height);
}

const struct fb_info* fb_info_get() {
    return &bootinfo_get()->fb_info;
}

uint32_t* fb_buffer_get(void) {
    return g_fb;
}
