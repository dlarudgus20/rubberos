#pragma once

#include "interrupt.h"
#include "keycode.h"
#include "ps2.h"

void hid_init(void);

void hid_on_keyboard(struct ps2_keyevent evt, struct ps2_char c);
void hid_on_mouse(struct ps2_mouse_event evt);

void intr_msg_on_keyboard(const struct intr_msg* msg);
void intr_msg_on_mouse(const struct intr_msg* msg);
