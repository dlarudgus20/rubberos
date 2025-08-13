#include <stdarg.h>
#include "freec/assert.h"
#include "freec/stdio.h"

#if __STDC_HOSTED__

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

noreturn void panic_impl(const char *msg, const char *file, const char *func, unsigned line) {
    fprintf(stderr, "[%s:%s:%d] %s\n", file, func, line, msg);
    raise(SIGINT);
    abort();
}

#endif

noreturn void panic_format(const char *fmt, const char *file, const char *func, unsigned line, ...) {
    va_list va;
    char buf[4096];
    va_start(va, line);
    vsnprintf(buf, sizeof(buf), fmt, va);
    panic_impl(buf, file, func, line);
    va_end(va);
}
