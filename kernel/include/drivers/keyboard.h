#pragma once

#include "interrupt.h"
#include "keyboard/keycode.h"

void keyboard_init(void);
void intr_msg_on_keyboard(const struct intr_msg* msg);
