#pragma once

#include <stddef.h>
#include <stdint.h>

inline size_t szdiv_ceil(size_t x, size_t y) {
    return x / y + (x % y != 0 ? 1 : 0);
}

inline uintptr_t uptrdiv_ceil(uintptr_t x, uintptr_t y) {
    return x / y + (x % y != 0 ? 1 : 0);
}

#define objectof(ptr, type, member) ((type*)((char*)(ptr) - offsetof(type, member)))
