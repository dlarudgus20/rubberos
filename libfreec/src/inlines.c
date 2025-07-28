#include "freec/stdlib.h"
#include "freec/string.h"

extern inline size_t szdiv_ceil(size_t x, size_t y);
extern inline uintptr_t uptrdiv_ceil(uintptr_t x, uintptr_t y);

extern inline void* memchr_not(const void* ptr, int ch, size_t count);

#if !__STDC_HOSTED__

extern inline void* memchr(const void* ptr, int ch, size_t count);

#endif
