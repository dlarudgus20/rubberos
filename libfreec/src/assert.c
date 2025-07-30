#include "freec/assert.h"

#if __STDC_HOSTED__

#include <stdio.h>
#include <stdlib.h>

noreturn void panic_impl(const char *msg, const char *file, const char *func, unsigned line) {
    fprintf(stderr, "[%s:%s:%d] %s\n", file, func, line, msg);
    abort();
}

#endif
