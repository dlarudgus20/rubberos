#pragma once

#include <stdbool.h>

struct size {
    int width, height;
};

struct rect {
    int x, y, width, height;
};

bool rect_is_in(const struct rect* r, int x, int y);
struct rect rect_intersect(const struct rect* r1, const struct rect* r2);
