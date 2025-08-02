#include <stdint.h>

#include "boot.h"
#include "kmain.h"

static struct bootinfo g_bootinfo;

struct bootinfo_arch {
    char cmd[BOOTINFO_CMD_MAXLEN];

    struct mmap* mmap;

    uint32_t fb_addr;
    uint32_t fb_pitch;
    uint32_t fb_width;
    uint32_t fb_height;
    uint8_t fb_bpp;
    uint8_t fb_type;
    uint16_t fb_reserved;
};

extern struct bootinfo_arch g_bootinfo_arch;

const struct bootinfo* bootinfo_get(void) {
    return &g_bootinfo;
}

void kmain_arch(volatile void* video) {
    g_bootinfo.cmd = g_bootinfo_arch.cmd;
    g_bootinfo.mmap = g_bootinfo_arch.mmap;

    g_bootinfo.fb_info.addr = video;
    g_bootinfo.fb_info.pitch = g_bootinfo_arch.fb_pitch;
    g_bootinfo.fb_info.width = g_bootinfo_arch.fb_width;
    g_bootinfo.fb_info.height = g_bootinfo_arch.fb_height;
    g_bootinfo.fb_info.bpp = g_bootinfo_arch.fb_bpp;
    g_bootinfo.fb_info.type = g_bootinfo_arch.fb_type;

    kmain();
}
