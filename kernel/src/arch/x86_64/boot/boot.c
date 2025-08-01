#include "boot.h"
#include "../../../kmain.h"

static struct bootinfo g_bootinfo;

const struct bootinfo* bootinfo_get(void) {
    return &g_bootinfo;
}

void kmain_arch(volatile int* video, int width, int height) {
    g_bootinfo.video = video;
    g_bootinfo.width = width;
    g_bootinfo.height = height;
    kmain();
}
