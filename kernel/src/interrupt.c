#include <collections/ringbuffer.h>

#include "interrupt.h"

#define INTR_QUEUE_SIZE 4096

static struct intr_msg g_msg_buffer[INTR_QUEUE_SIZE];
static struct ringbuffer g_intr_queue;

void interrupt_init(void) {
    descriptor_init();

    ringbuffer_init(&g_intr_queue, g_msg_buffer, INTR_QUEUE_SIZE);
}

bool intr_queue_push(struct intr_msg* msg) {
    if (ringbuffer_is_full(&g_intr_queue)) {
        return false;
    }
    ringbuffer_push(&g_intr_queue, struct intr_msg, *msg);
    return true;
}

bool intr_queue_try_pop(struct intr_msg* msg) {
    if (ringbuffer_is_empty(&g_intr_queue)) {
        return false;
    }
    *msg = ringbuffer_pop(&g_intr_queue, struct intr_msg);

    return true;
}
