#pragma once

#if !__STDC_HOSTED__

#include <stddef.h>
#include <stdarg.h>

#if __has_attribute(format)
#define SNPRINTF_FORMAT_ATTRIB __attribute__((format(printf, 3, 4)))
#define VSNPRINTF_FORMAT_ATTRIB __attribute__((format(printf, 3, 0)))
#else
#define SNPRINTF_FORMAT_ATTRIB
#define VSNPRINTF_FORMAT_ATTRIB
#endif

int snprintf(char *buf, size_t size, const char *format, ...) SNPRINTF_FORMAT_ATTRIB;
int vsnprintf(char *buf, size_t size, const char *format, va_list va) VSNPRINTF_FORMAT_ATTRIB;

#else
#include <stdio.h>
#endif
