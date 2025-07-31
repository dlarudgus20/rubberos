#include "freec/stdlib.h"
#include "freec/string.h"
#include "freec/ctype.h"

extern inline size_t szdiv_ceil(size_t x, size_t y);
extern inline uintptr_t uptrdiv_ceil(uintptr_t x, uintptr_t y);

extern inline void* memchr_not(const void* ptr, int ch, size_t count);

#if !__STDC_HOSTED__

extern inline void* memchr(const void* ptr, int ch, size_t count);
extern inline size_t strnlen(const char* str, size_t strsz);
extern inline size_t strnlen_s(const char* str, size_t strsz);

extern inline int isdigit(int ch);

#endif
