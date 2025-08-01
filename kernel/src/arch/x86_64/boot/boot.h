#pragma once

struct bootinfo {
    volatile int* video;
    int width;
    int height;
    void* tags;
};

const struct bootinfo* bootinfo_get(void);
