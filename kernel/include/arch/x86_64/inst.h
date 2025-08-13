#pragma once

#include <stddef.h>
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

inline void load_gdt(void* gdt, size_t size) {
    struct {
        uint16_t size;
        void* base;
    } __attribute__((packed)) gdtr = { (uint16_t)(size - 1), gdt };
    __asm__ __volatile__ ( "lgdt [%0]" : : "m"(gdtr) );
}

inline void load_idt(void* idt, size_t size) {
    struct {
        uint16_t size;
        void* base;
    } __attribute__((packed)) idtr = { (uint16_t)(size - 1), idt };
    __asm__ __volatile__ ( "lidt [%0]" : : "m"(idtr) );
}

inline void load_tss(uint16_t selector) {
    __asm__ __volatile__ ( "ltr %0" : : "r"(selector) );
}
