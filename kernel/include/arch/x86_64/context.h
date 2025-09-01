#pragma once

#include "./interrupt.h"

struct context {
    ISR_STACKFRAME_PUSH
    ISR_STACKFRAME_IRETQ
};

#define PRI_CONTEXT         PRI_ISR_STACKFRAME
#define ARG_CONTEXT(ctx)    ARG_ISR_STACKFRAME(ctx)

void context_switch(struct context* from, const struct context* to);
