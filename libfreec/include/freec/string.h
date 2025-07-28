#pragma once

#if !__STDC_HOSTED__

#define memcpy(dest, src, count) __builtin_memcpy(dest, src, count)
#define memmove(dest, src, count) __builtin_memmove(dest, src, count)
#define memset(dest, ch, count) __builtin_memset(dest, ch, count)
#define memcmp(lhs, rhs, count) __builtin_memcmp(lhs, rhs, count)

inline void* memchr(const void* ptr, int ch, size_t count) {
    for (const char* p = ptr; count-- > 0 && *p != ch; p++) {
        if (*p == ch) {
            return (void*)p;
        }
    }
    return 0;
}

#else
#include <string.h>
#endif

inline void* memchr_not(const void* ptr, int ch, size_t count) {
    for (const char* p = ptr; count-- > 0 && *p != ch; p++) {
        if (*p != ch) {
            return (void*)p;
        }
    }
    return 0;
}
