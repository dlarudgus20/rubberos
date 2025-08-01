#pragma once

struct bootinfo {
    volatile int* video;
    int width;
    int height;
};

const struct bootinfo* bootinfo_get(void);
