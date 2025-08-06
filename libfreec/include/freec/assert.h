#pragma once

#include <stdnoreturn.h>

#if __STDC_HOSTED__
#include <assert.h>
#undef assert // to override standard assert()
#elif __STDC_VERSION__ < 202311l
#define static_assert _Static_assert
#endif

#define panic(msg) (panic_impl((#msg)[0] ? "panic : " msg : "panic", __FILE__, __func__, __LINE__))

#define ASSERT_1(exp, ...) ((void)((exp) || (panic_impl("assertion failed : " #exp, __FILE__, __func__, __LINE__), 1)))
#define ASSERT_2(exp, msg) ((void)((exp) || (panic_impl(msg " : " #exp, __FILE__, __func__, __LINE__), 1)))

#define ASSERT_EXPAND(exp, msg, dummy, impl, ...) impl(exp, msg)
#define assert(...) ASSERT_EXPAND(__VA_ARGS__, , ASSERT_2, ASSERT_1, )

#if __has_attribute(format)
#define PANIC_FORMAT_ATTRIB __attribute__((format(printf, 1, 5)))
#else
#define PANIC_FORMAT_ATTRIB
#endif

#define assertf(exp, msg, ...) ((void)((exp) || (panic_format(msg " : %s", __FILE__, __func__, __LINE__, __VA_ARGS__, #exp), 1)))

noreturn void panic_impl(const char *msg, const char *file, const char *func, unsigned line);
noreturn void panic_format(const char *fmt, const char *file, const char *func, unsigned line, ...) PANIC_FORMAT_ATTRIB;
