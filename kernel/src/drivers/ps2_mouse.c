#include <freec/string.h>
#include "tty.h"
#include "drivers/ps2.h"

void ps2_mouse_init(struct ps2_mouse* ms) {
    memset(ms, 0, sizeof(struct ps2_mouse));
}

bool ps2_mouse_put_byte(struct ps2_mouse* ms, uint8_t byte, struct ps2_mouse_event* evt) {
    ms->packet[ms->packet_index] = byte;
    ms->packet_index++;

    if (ms->packet_index < 3) {
        return false;
    }

    ms->packet_index = 0;
    uint8_t flags = ms->packet[0];

    evt->left = (flags & 0x01) != 0;
    evt->right = (flags & 0x02) != 0;
    evt->middle = (flags & 0x04) != 0;

    evt->dx = (int8_t)(ms->packet[1] | (flags & 0x10 ? 0xff00 : 0));
    evt->dy = (int8_t)(ms->packet[2] | (flags & 0x20 ? 0xff00 : 0));

    return true;
}
