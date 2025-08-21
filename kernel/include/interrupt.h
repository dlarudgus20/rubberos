#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "arch/interrupt.h"

enum intr_msg_type {
    INTR_MSG_KEYBOARD,
    INTR_MSG_MOUSE,
};

struct intr_msg {
    uint8_t type;
    uint8_t data;
};

void interrupt_init(void);
void interrupt_register_isr(uint8_t vector, void (*handler)());

void interrupt_device_init(void);
void interrupt_device_enable(void);

bool intr_queue_push(struct intr_msg* msg);
bool intr_queue_try_pop(struct intr_msg* msg);
