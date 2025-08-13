#pragma once

#include <stddef.h>
#include <stdint.h>

inline size_t szdiv_ceil(size_t x, size_t y) {
    return x / y + (x % y != 0 ? 1 : 0);
}

inline uintptr_t uptrdiv_ceil(uintptr_t x, uintptr_t y) {
    return x / y + (x % y != 0 ? 1 : 0);
}

void sort(void* ptr, size_t count, size_t size, int (*comp)(const void*, const void*));

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define container_of(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))
