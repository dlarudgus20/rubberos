#include "freec/ctype.h"

#if !__STDC_HOSTED__

extern inline int isdigit(int ch);

#else
typedef int dummy;
#endif
