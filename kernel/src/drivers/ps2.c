#include <freec/assert.h>

#include "drivers/ps2.h"

#define SC_EXT1 0xe0
#define SC_EXT2 0xe1
#define SC_PRTSCR_DOWN1 0x2a
#define SC_PRTSCR_DOWN2 0x37
#define SC_PRTSCR_UP1   0xb7
#define SC_PRTSCR_UP2   0xaa
#define SC_PAUSE_DOWN   0x1d45
#define SC_PAUSE_UP     0x9dc5

enum {
    STATE_START,
    STATE_EXT1,
    STATE_EXT1_1,
    STATE_EXT1_2,
    STATE_EXT2_1,
    STATE_EXT2_2
};

void ps2_keyboard_init(struct ps2_keyboard* kb) {
    kb->state = STATE_START;
    kb->data = 0;
    kb->lshift = kb->rshift = false;
    kb->lctrl = kb->rctrl = false;
    kb->lalt = kb->ralt = false;
    kb->caps = false;
    kb->scroll = false;
    kb->num = false;
}

static keycode_t keycode_normal(uint8_t byte) {
    switch (byte) {
        case 0x01: return KEY_ESCAPE;
        case 0x02: return KEY_1;
        case 0x03: return KEY_2;
        case 0x04: return KEY_3;
        case 0x05: return KEY_4;
        case 0x06: return KEY_5;
        case 0x07: return KEY_6;
        case 0x08: return KEY_7;
        case 0x09: return KEY_8;
        case 0x0a: return KEY_9;
        case 0x0b: return KEY_0;
        case 0x0c: return KEY_MINUS;
        case 0x0d: return KEY_PLUS;
        case 0x0e: return KEY_BACKSPACE;
        case 0x0f: return KEY_TAB;
        case 0x10: return KEY_Q;
        case 0x11: return KEY_W;
        case 0x12: return KEY_E;
        case 0x13: return KEY_R;
        case 0x14: return KEY_T;
        case 0x15: return KEY_Y;
        case 0x16: return KEY_U;
        case 0x17: return KEY_I;
        case 0x18: return KEY_O;
        case 0x19: return KEY_P;
        case 0x1a: return KEY_OEM_LBRACE;
        case 0x1b: return KEY_OEM_RBRACE;
        case 0x1c: return KEY_ENTER;
        case 0x1d: return KEY_LCTRL;
        case 0x1e: return KEY_A;
        case 0x1f: return KEY_S;
        case 0x20: return KEY_D;
        case 0x21: return KEY_F;
        case 0x22: return KEY_G;
        case 0x23: return KEY_H;
        case 0x24: return KEY_J;
        case 0x25: return KEY_K;
        case 0x26: return KEY_L;
        case 0x27: return KEY_OEM_COLON;
        case 0x28: return KEY_OEM_QUOTE;
        case 0x29: return KEY_OEM_TILDE;
        case 0x2a: return KEY_LSHIFT;
        case 0x2b: return KEY_OEM_PIPE;
        case 0x2c: return KEY_Z;
        case 0x2d: return KEY_X;
        case 0x2e: return KEY_C;
        case 0x2f: return KEY_V;
        case 0x30: return KEY_B;
        case 0x31: return KEY_N;
        case 0x32: return KEY_M;
        case 0x33: return KEY_COMMA;
        case 0x34: return KEY_PERIOD;
        case 0x35: return KEY_OEM_SLASH;
        case 0x36: return KEY_RSHIFT;
        case 0x37: return KEY_NUMPAD_MUL;
        case 0x38: return KEY_LALT;
        case 0x39: return KEY_SPACE;
        case 0x3a: return KEY_CAPSLOCK;
        case 0x3b: return KEY_F1;
        case 0x3c: return KEY_F2;
        case 0x3d: return KEY_F3;
        case 0x3e: return KEY_F4;
        case 0x3f: return KEY_F5;
        case 0x40: return KEY_F6;
        case 0x41: return KEY_F7;
        case 0x42: return KEY_F8;
        case 0x43: return KEY_F9;
        case 0x44: return KEY_F10;
        case 0x45: return KEY_NUMLOCK;
        case 0x46: return KEY_SCROLLLOCK;
        case 0x47: return KEY_NUMPAD7;
        case 0x48: return KEY_NUMPAD8;
        case 0x49: return KEY_NUMPAD9;
        case 0x4a: return KEY_NUMPAD_SUB;
        case 0x4b: return KEY_NUMPAD4;
        case 0x4c: return KEY_NUMPAD5;
        case 0x4d: return KEY_NUMPAD6;
        case 0x4e: return KEY_NUMPAD_ADD;
        case 0x4f: return KEY_NUMPAD1;
        case 0x50: return KEY_NUMPAD2;
        case 0x51: return KEY_NUMPAD3;
        case 0x52: return KEY_NUMPAD0;
        case 0x53: return KEY_NUMPAD_PERIOD;
        case 0x57: return KEY_F11;
        case 0x58: return KEY_F12;
        default: return KEYCODE_UNKNOWN;
    }
}

static keycode_t keycode_extended(uint8_t byte) {
    switch (byte) {
        case 0x10: return KEY_MM_PREVTRACK;
        case 0x19: return KEY_MM_NEXTTRACK;
        case 0x1c: return KEY_NUMPAD_ENTER;
        case 0x1d: return KEY_LCTRL;
        case 0x20: return KEY_MM_MUTE;
        case 0x21: return KEY_MM_CALCULATOR;
        case 0x22: return KEY_MM_PLAYPAUSE;
        case 0x24: return KEY_MM_STOP;
        case 0x2e: return KEY_MM_VOLUMEDOWN;
        case 0x30: return KEY_MM_VOLUMEUP;
        case 0x32: return KEY_MM_WWWHOME;
        case 0x35: return KEY_NUMPAD_DIV;
        case 0x38: return KEY_RALT;
        case 0x47: return KEY_HOME;
        case 0x48: return KEY_UP;
        case 0x49: return KEY_PAGEUP;
        case 0x4b: return KEY_LEFT;
        case 0x4d: return KEY_RIGHT;
        case 0x4f: return KEY_END;
        case 0x50: return KEY_DOWN;
        case 0x51: return KEY_PAGEDOWN;
        case 0x52: return KEY_INSERT;
        case 0x53: return KEY_DELETE;
        case 0x5b: return KEY_LWIN;
        case 0x5c: return KEY_RWIN;
        case 0x5d: return KEY_APPS;
        case 0x5e: return KEY_ACPI_POWER;
        case 0x5f: return KEY_ACPI_SLEEP;
        case 0x63: return KEY_ACPI_WAKE;
        case 0x65: return KEY_MM_WWWSEARCH;
        case 0x66: return KEY_MM_WWWFAVORITES;
        case 0x67: return KEY_MM_WWWREFRESH;
        case 0x68: return KEY_MM_WWWSTOP;
        case 0x69: return KEY_MM_WWWFORWARD;
        case 0x6a: return KEY_MM_WWWBACK;
        case 0x6b: return KEY_MM_MYCOMPUTER;
        case 0x6c: return KEY_MM_EMAIL;
        case 0x6d: return KEY_MM_MEDIASELECT;
        default: return KEYCODE_UNKNOWN;
    }
}

bool ps2_keyboard_put_byte(struct ps2_keyboard* kb, uint8_t byte, struct ps2_keyevent* evt) {
    if (kb->state == STATE_EXT1_1) {
        if (byte == 0xe0) {
            kb->state = STATE_EXT1_2;
            return false;
        } else {
            // something goes wrong; reset state
            kb->state = STATE_START;
        }
    }

    switch (kb->state) {
        case STATE_START:
            if (byte == SC_EXT1) {
                kb->state = STATE_EXT1;
                return false;
            } else if (byte == SC_EXT2) {
                kb->state = STATE_EXT2_1;
                return false;
            } else if (byte & 0x80) {
                keycode_t keycode = keycode_normal(byte & 0x7f);
                if (keycode == KEYCODE_UNKNOWN) {
                    return false;
                }
                evt->keycode = keycode;
                evt->keydown = false;
                return true;
            } else {
                keycode_t keycode = keycode_normal(byte);
                if (keycode == KEYCODE_UNKNOWN) {
                    return false;
                }
                evt->keycode = keycode;
                evt->keydown = true;
                return true;
            }
        case STATE_EXT1:
            if (byte == SC_PRTSCR_DOWN1 || byte == SC_PRTSCR_UP1) {
                kb->state = STATE_EXT1_1;
                kb->data = byte;
                return false;
            } if (byte & 0x80) {
                kb->state = STATE_START;
                keycode_t keycode = keycode_extended(byte & 0x7f);
                if (keycode == KEYCODE_UNKNOWN) {
                    return false;
                }
                evt->keycode = keycode;
                evt->keydown = false;
                return true;
            } else {
                kb->state = STATE_START;
                keycode_t keycode = keycode_extended(byte);
                if (keycode == KEYCODE_UNKNOWN) {
                    return false;
                }
                evt->keycode = keycode;
                evt->keydown = true;
                return true;
            }
        case STATE_EXT1_2:
            kb->state = STATE_START;
            if (kb->data == SC_PRTSCR_UP1 && byte == SC_PRTSCR_UP2) {
                evt->keycode = KEY_PRINTSCREEN;
                evt->keydown = false;
                return true;
            } else if (kb->data == SC_PRTSCR_DOWN1 && byte == SC_PRTSCR_DOWN2) {
                evt->keycode = KEY_PRINTSCREEN;
                evt->keydown = true;
                return true;
            } else {
                return false;
            }
        case STATE_EXT2_1:
            kb->state = STATE_EXT2_2;
            kb->data = byte;
            return false;
        case STATE_EXT2_2: {
            kb->state = STATE_START;
            uint16_t data = (kb->data << 8) | byte;
            if (data == SC_PAUSE_UP) {
                evt->keycode = KEY_PAUSEBREAK;
                evt->keydown = false;
                return true;
            } else if (data == SC_PAUSE_DOWN) {
                evt->keycode = KEY_PAUSEBREAK;
                evt->keydown = true;
                return true;
            } else {
                return false;
            }
        }
        default:
            panic();
    }
}


struct ps2_char ps2_keyboard_process_keyevent(struct ps2_keyboard* kb, const struct ps2_keyevent* evt) {
#define PS2RAW(k) (struct ps2_char){ .raw = true, .ch = 0, .keycode = (k) }
#define PS2CHAR(c) (struct ps2_char){ .raw = false, .ch = (c), .keycode = evt->keycode }
    const struct ps2_char raw = PS2RAW(evt->keycode);

    const bool control = kb->lctrl || kb->rctrl || kb->lalt || kb->ralt;
    const bool shift = kb->lshift || kb->rshift;
    const bool upper = kb->caps ^ shift;

    switch (evt->keycode) {
        case KEY_LSHIFT: kb->lshift = evt->keydown; break;
        case KEY_RSHIFT: kb->rshift = evt->keydown; break;
        case KEY_LCTRL: kb->lctrl = evt->keydown; break;
        case KEY_RCTRL: kb->rctrl = evt->keydown; break;
        case KEY_LALT: kb->lalt = evt->keydown; break;
        case KEY_RALT: kb->ralt = evt->keydown; break;

        case KEY_CAPSLOCK:
            if (evt->keydown) {
                kb->caps = !kb->caps;
            }
            break;
        case KEY_SCROLLLOCK:
            if (evt->keydown) {
                kb->scroll = !kb->scroll;
            }
            break;
        case KEY_NUMLOCK:
            if (evt->keydown) {
                kb->num = !kb->num;
            }
            break;
    }

    if (!control) {
        switch (evt->keycode) {
            case KEY_OEM_TILDE:     return PS2CHAR(shift ? '~' : '`');
            case KEY_1:             return PS2CHAR(shift ? '!' : '1');
            case KEY_2:             return PS2CHAR(shift ? '@' : '2');
            case KEY_3:             return PS2CHAR(shift ? '#' : '3');
            case KEY_4:             return PS2CHAR(shift ? '$' : '4');
            case KEY_5:             return PS2CHAR(shift ? '%' : '5');
            case KEY_6:             return PS2CHAR(shift ? '^' : '6');
            case KEY_7:             return PS2CHAR(shift ? '&' : '7');
            case KEY_8:             return PS2CHAR(shift ? '*' : '8');
            case KEY_9:             return PS2CHAR(shift ? '(' : '9');
            case KEY_0:             return PS2CHAR(shift ? ')' : '0');
            case KEY_MINUS:         return PS2CHAR(shift ? '_' : '-');
            case KEY_PLUS:          return PS2CHAR(shift ? '+' : '=');
            case KEY_Q:             return PS2CHAR(upper ? 'Q' : 'q');
            case KEY_W:             return PS2CHAR(upper ? 'W' : 'w');
            case KEY_E:             return PS2CHAR(upper ? 'E' : 'e');
            case KEY_R:             return PS2CHAR(upper ? 'R' : 'r');
            case KEY_T:             return PS2CHAR(upper ? 'T' : 't');
            case KEY_Y:             return PS2CHAR(upper ? 'Y' : 'y');
            case KEY_U:             return PS2CHAR(upper ? 'U' : 'u');
            case KEY_I:             return PS2CHAR(upper ? 'I' : 'i');
            case KEY_O:             return PS2CHAR(upper ? 'O' : 'o');
            case KEY_P:             return PS2CHAR(upper ? 'P' : 'p');
            case KEY_OEM_LBRACE:    return PS2CHAR(shift ? '{' : '[');
            case KEY_OEM_RBRACE:    return PS2CHAR(shift ? '}' : ']');
            case KEY_OEM_PIPE:      return PS2CHAR(shift ? '|' : '\\');
            case KEY_A:             return PS2CHAR(upper ? 'A' : 'a');
            case KEY_S:             return PS2CHAR(upper ? 'S' : 's');
            case KEY_D:             return PS2CHAR(upper ? 'D' : 'd');
            case KEY_F:             return PS2CHAR(upper ? 'F' : 'f');
            case KEY_G:             return PS2CHAR(upper ? 'G' : 'g');
            case KEY_H:             return PS2CHAR(upper ? 'H' : 'h');
            case KEY_J:             return PS2CHAR(upper ? 'J' : 'j');
            case KEY_K:             return PS2CHAR(upper ? 'K' : 'k');
            case KEY_L:             return PS2CHAR(upper ? 'L' : 'l');
            case KEY_OEM_COLON:     return PS2CHAR(shift ? ':' : ';');
            case KEY_OEM_QUOTE:     return PS2CHAR(shift ? '"' : '\'');
            case KEY_Z:             return PS2CHAR(upper ? 'Z' : 'z');
            case KEY_X:             return PS2CHAR(upper ? 'X' : 'x');
            case KEY_C:             return PS2CHAR(upper ? 'C' : 'c');
            case KEY_V:             return PS2CHAR(upper ? 'V' : 'v');
            case KEY_B:             return PS2CHAR(upper ? 'B' : 'b');
            case KEY_N:             return PS2CHAR(upper ? 'N' : 'n');
            case KEY_M:             return PS2CHAR(upper ? 'M' : 'm');
            case KEY_COMMA:         return PS2CHAR(shift ? '<' : ',');
            case KEY_PERIOD:        return PS2CHAR(shift ? '>' : '.');
            case KEY_OEM_SLASH:     return PS2CHAR(shift ? '?' : '/');
            case KEY_SPACE:         return PS2CHAR(' ');
            case KEY_TAB:           return PS2CHAR('\t');
            case KEY_ENTER:         return PS2CHAR('\n');
            case KEY_BACKSPACE:     return PS2CHAR('\b');

            case KEY_NUMPAD_MUL:    return PS2CHAR('*');
            case KEY_NUMPAD_DIV:    return PS2CHAR('/');
            case KEY_NUMPAD_ADD:    return PS2CHAR('+');
            case KEY_NUMPAD_SUB:    return PS2CHAR('-');
            case KEY_NUMPAD_ENTER:  return PS2CHAR('\n');
        }
    }
    
    switch (evt->keycode) {
        case KEY_NUMPAD0:       return kb->num ? (control ? raw : PS2CHAR('0')) : PS2RAW(KEY_INSERT);
        case KEY_NUMPAD1:       return kb->num ? (control ? raw : PS2CHAR('1')) : PS2RAW(KEY_END);
        case KEY_NUMPAD2:       return kb->num ? (control ? raw : PS2CHAR('2')) : PS2RAW(KEY_DOWN);
        case KEY_NUMPAD3:       return kb->num ? (control ? raw : PS2CHAR('3')) : PS2RAW(KEY_PAGEDOWN);
        case KEY_NUMPAD4:       return kb->num ? (control ? raw : PS2CHAR('4')) : PS2RAW(KEY_LEFT);
        case KEY_NUMPAD5:       return kb->num && !control ? PS2CHAR('5') : raw;
        case KEY_NUMPAD6:       return kb->num ? (control ? raw : PS2CHAR('6')) : PS2RAW(KEY_RIGHT);
        case KEY_NUMPAD7:       return kb->num ? (control ? raw : PS2CHAR('7')) : PS2RAW(KEY_HOME);
        case KEY_NUMPAD8:       return kb->num ? (control ? raw : PS2CHAR('8')) : PS2RAW(KEY_UP);
        case KEY_NUMPAD9:       return kb->num ? (control ? raw : PS2CHAR('9')) : PS2RAW(KEY_PAGEUP);
        case KEY_NUMPAD_PERIOD: return kb->num ? (control ? raw : PS2CHAR('.')) : PS2RAW(KEY_PERIOD);
    }

    return raw;
}
