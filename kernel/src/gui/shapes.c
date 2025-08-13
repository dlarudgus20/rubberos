#include <freec/stdlib.h>
#include "gui/shapes.h"

bool rect_is_in(const struct rect* r, int x, int y) {
    return r->x <= x && x < r->x + r->width
        && r->y <= y && y < r->y + r->height;
}

struct rect rect_intersect(const struct rect* r1, const struct rect* r2) {
    struct rect r;
    r.x = MAX(r1->x, r2->x);
    r.y = MAX(r1->y, r2->y);
    r.width = MIN(r1->x + r1->width, r2->x + r2->width) - r.x;
    r.height = MIN(r1->y + r1->height, r2->y + r2->height) - r.y;
    if (r.width < 0 || r.height < 0) {
        r.width = 0;
        r.height = 0;
    }
    return r;
}
