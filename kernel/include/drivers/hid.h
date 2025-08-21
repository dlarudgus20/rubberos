#pragma once

#include "interrupt.h"
#include "keycode.h"

void hid_init(void);

void intr_msg_on_keyboard(const struct intr_msg* msg);
void intr_msg_on_mouse(const struct intr_msg* msg);
