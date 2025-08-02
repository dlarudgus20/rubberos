#pragma once

#if !__STDC_HOSTED__

#include <stddef.h>

void* memcpy(void* restrict dest, const void* restrict src, size_t count);
void* memmmove(void* dest, const void* src, size_t count);
void* memset(void* dest, int ch, size_t count);
int memcmp(const void* lhs, const void* rhs, size_t count);
void* memchr(const void* ptr, int ch, size_t count);

size_t strnlen(const char* str, size_t strsz);
size_t strnlen_s(const char* str, size_t strsz);

#ifndef NO_BUILTIN_MACRO
#define memcpy(dest, src, count) __builtin_memcpy(dest, src, count)
#define memmove(dest, src, count) __builtin_memmove(dest, src, count)
#define memset(dest, ch, count) __builtin_memset(dest, ch, count)
#define memcmp(lhs, rhs, count) __builtin_memcmp(lhs, rhs, count)
#endif

#else
#include <string.h>
#endif

void* memchr_not(const void* ptr, int ch, size_t count);
