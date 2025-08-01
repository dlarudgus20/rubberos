#pragma once

#include <stdint.h>

inline void out8(uint16_t port, uint8_t data) {
    __asm__ __volatile__ ( "out dx, al" : : "Nd"(port), "a"(data) : "memory" );
}

inline uint8_t in8(uint16_t port) {
    uint8_t data;
    __asm__ __volatile__ ( "in al, dx" : "=a"(data) : "Nd"(port) : "memory" );
    return data;
}
