#include "drivers/hid.h"
#include "tty.h"
#include "gui/evt.h"

#include "memory.h"
#include "arch/context.h"
#include "arch/inst.h"
#include <freec/assert.h>

struct ctxdata {
    char stack[4096];
    struct context this, main;
    int avr;
};

static void testtask_main(struct ctxdata* arg) {
    tty0_printf("hello task(avr=%d)\n", arg->avr);
    for (unsigned count = 1; ; count++) {
        tty0_printf("task loop #%u, rsp=%#018lx\n", count, arg->this.rsp);
        context_switch(&arg->this, &arg->main);
    }
}

static __attribute__((unused)) void testtask(bool quit) {
    static struct ctxdata* ptr = NULL;

    if (quit) {
        if (ptr) {
            tty0_printf("terminate task\n");
            dynmem_dealloc(ptr, sizeof(struct ctxdata));
            ptr = NULL;
        }
    } else {
        if (!ptr) {
            ptr = dynmem_alloc(sizeof(struct ctxdata)).ptr;
            assert(ptr);

            ptr->this.rip = (uint64_t)testtask_main;
            ptr->this.cs = 8;
            ptr->this.rflags = rflags_get();
            ptr->this.rsp = (uint64_t)ptr->stack;
            ptr->this.rbp = ptr->this.rsp;
            ptr->this.ss = 16;
            ptr->this.ds = 16;
            ptr->this.es = 16;
            ptr->this.fs = 0;
            ptr->this.gs = 0;
            ptr->this.rdi = (uint64_t)ptr;
            ptr->avr = 42;
        }

        context_switch(&ptr->main, &ptr->this);
    }
}

void hid_on_keyboard(struct hid_keyevent evt, struct hid_char c) {
    gui_on_keyboard(evt, c);
}

void hid_on_mouse(struct hid_mouse_event evt) {
    gui_on_mouse(evt);
}
