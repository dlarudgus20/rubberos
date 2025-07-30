#pragma once

#include <stdnoreturn.h>

#if __STDC_HOSTED__
#include <assert.h> // to override standard assertion
#undef assert
#endif

#define panic(msg) (panic_impl(msg, "", __FILE__, __func__, __LINE__))
#define ASSERT_1(exp, ...) ((void)((exp) || (panic_impl("assertion failed : " #exp, __FILE__, __func__, __LINE__), 1)))
#define ASSERT_2(exp, msg) ((void)((exp) || (panic_impl(msg " : " #exp, __FILE__, __func__, __LINE__), 1)))

#define ASSERT_EXPAND(exp, msg, dummy, impl, ...) impl(exp, msg)
#define assert(...) ASSERT_EXPAND(__VA_ARGS__, , ASSERT_2, ASSERT_1, )

noreturn void panic_impl(const char *msg, const char *file, const char *func, unsigned line);
