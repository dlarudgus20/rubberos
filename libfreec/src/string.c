#define NO_BUILTIN_MACRO
#include "freec/string.h"

#if !__STDC_HOSTED__

void* memcpy(void* restrict dest, const void* restrict src, size_t count) {
    const char* restrict s = src;
    for (char* restrict d = dest; count-- > 0; d++, s++) {
        *d = *s;
    }
    return dest;
}

void* memmove(void* dest, const void* src, size_t count) {
    const char* s = src;
    char* d = dest;
    if (d <= s) {
        for (; count-- > 0; d++, s++) {
            *d = *s;
        }
    } else {
        for (d += count - 1, s+= count - 1; count-- > 0; d--, s--) {
            *d = *s;
        }
    }
    return dest;
}

void* memset(void* dest, int ch, size_t count) {
    for (unsigned char* d = dest; count-- > 0; d++) {
        *d = (unsigned char)ch;
    }
    return dest;
}

int memcmp(const void* lhs, const void* rhs, size_t count) {
    for (const signed char *l = lhs, *r = rhs; count-- > 0; l++, r++) {
        signed char diff = *l - *r;
        if (diff != 0) {
            return diff;
        }
    }
    return 0;
}

void* memchr(const void* ptr, int ch, size_t count) {
    for (const unsigned char* p = ptr; count-- > 0; p++) {
        if (*p == (unsigned char)ch) {
            return (void*)p;
        }
    }
    return 0;
}

size_t strnlen(const char* str, size_t strsz) {
    size_t l = 0;
    for (; l < strsz && str[l] != 0; l++) {}
    return l;
}

size_t strnlen_s(const char* str, size_t strsz) {
    if (!str) {
        return 0;
    }
    size_t l = 0;
    for (; l < strsz && str[l] != 0; l++) {}
    return l;
}

#endif

void* memchr_not(const void* ptr, int ch, size_t count) {
    for (const unsigned char* p = ptr; count-- > 0; p++) {
        if (*p != (unsigned char)ch) {
            return (void*)p;
        }
    }
    return 0;
}
