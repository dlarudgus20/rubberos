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

inline void wait_for_int(void) {
    __asm__ __volatile__ ( "hlt" );
}

inline void pagetable_set(uintptr_t pagetable_phys) {
    __asm__ __volatile__ ( "mov cr3, rax" : : "a"(pagetable_phys) : "memory" );
}

inline void tlb_flush_all(void) {
    __asm__ __volatile__ ( "mov rax, cr3 \t\n mov cr3, rax" : : : "rax", "memory" );
}

inline void tlb_flush_for(void* virt) {
    __asm__ __volatile__ ( "invlpg [%0]" : : "r"(virt) : "memory" );
}
