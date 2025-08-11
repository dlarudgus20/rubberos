#include <stdint.h>

#include "boot.h"
#include "kmain.h"

static struct bootinfo g_bootinfo;

struct bootinfo_arch {
    char cmd[BOOTINFO_CMD_MAXLEN];
    struct mmap* mmap;
    struct fb_info fb_info;
};

extern struct bootinfo_arch g_bootinfo_arch;

const struct bootinfo* bootinfo_get(void) {
    return &g_bootinfo;
}

void kmain_arch(void) {
    g_bootinfo.cmd = g_bootinfo_arch.cmd;
    g_bootinfo.mmap = g_bootinfo_arch.mmap;
    g_bootinfo.fb_info = g_bootinfo_arch.fb_info;
    kmain();
}
