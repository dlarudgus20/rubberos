#pragma once

#if !__STDC_HOSTED__

inline int isdigit(int ch) {
    return '0' <= ch && ch <= '9';
}

#else
#include <ctype.h>
#endif
