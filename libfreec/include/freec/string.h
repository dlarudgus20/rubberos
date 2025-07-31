#pragma once

#if !__STDC_HOSTED__

#include <stddef.h>

#define memcpy(dest, src, count) __builtin_memcpy(dest, src, count)
#define memmove(dest, src, count) __builtin_memmove(dest, src, count)
#define memset(dest, ch, count) __builtin_memset(dest, ch, count)
#define memcmp(lhs, rhs, count) __builtin_memcmp(lhs, rhs, count)

inline void* memchr(const void* ptr, int ch, size_t count) {
    for (const char* p = ptr; count-- > 0; p++) {
        if ((unsigned char)*p == (unsigned char)ch) {
            return (void*)p;
        }
    }
    return 0;
}

inline size_t strnlen(const char* str, size_t strsz) {
    size_t l = 0;
    for (; l < strsz && str[l] != 0; l++) {}
    return l;
}

inline size_t strnlen_s(const char* str, size_t strsz) {
    if (!str) {
        return 0;
    }
    size_t l = 0;
    for (; l < strsz && str[l] != 0; l++) {}
    return l;
}

#else
#include <string.h>
#endif

inline void* memchr_not(const void* ptr, int ch, size_t count) {
    for (const char* p = ptr; count-- > 0; p++) {
        if ((unsigned char)*p != (unsigned char)ch) {
            return (void*)p;
        }
    }
    return 0;
}
