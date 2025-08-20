#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#if __has_attribute(always_inline)
#define ALWAYS_INLINE static inline __attribute__((always_inline))
#else
#define ALWAYS_INLINE static inline
#endif

#define RFLAGS_CARRY    (1 << 0)
#define RFLAGS_PARITY   (1 << 1)
#define RFLAGS_AUXCARRY (1 << 4)
#define RFLAGS_ZERO     (1 << 6)
#define RFLAGS_SIGN     (1 << 7)
#define RFLAGS_TRAP     (1 << 8)
#define RFLAGS_INTR     (1 << 9)
#define RFLAGS_DIR      (1 << 10)
#define RFLAGS_OVERFLOW (1 << 11)

ALWAYS_INLINE void compiler_barrier(void) {
    __asm__ __volatile__ ( "" : : : "memory" );
}

ALWAYS_INLINE void out8(uint16_t port, uint8_t data) {
    __asm__ __volatile__ ( "out dx, al" : : "Nd"(port), "a"(data) : "memory" );
}

ALWAYS_INLINE uint8_t in8(uint16_t port) {
    uint8_t data;
    __asm__ __volatile__ ( "in al, dx" : "=a"(data) : "Nd"(port) : "memory" );
    return data;
}

ALWAYS_INLINE uint64_t rflags_get(void) {
    uint64_t flags;
    __asm__ __volatile__ ( "pushfq; pop %0" : "=r"(flags) );
    return flags;
}

ALWAYS_INLINE void rflags_set(uint64_t flags) {
    __asm__ __volatile__ ( "push %0; popfq" : : "r"(flags) );
}

ALWAYS_INLINE bool interrupt_is_enabled(void) {
    return (rflags_get() & RFLAGS_INTR) != 0;
}

ALWAYS_INLINE void interrupt_disable(void) {
    __asm__ __volatile__ ( "cli" );
}

ALWAYS_INLINE void interrupt_enable(void) {
    __asm__ __volatile__ ( "sti" );
}

ALWAYS_INLINE void wait_for_intr(void) {
    __asm__ __volatile__ ( "hlt" );
}

ALWAYS_INLINE void interrupt_enable_and_wait(void) {
    __asm__ __volatile__ ( "sti; hlt" );
}

ALWAYS_INLINE void spinloop_hint(void) {
    __asm__ __volatile__ ( "pause" );
}

ALWAYS_INLINE void pagetable_set(uintptr_t pagetable_phys) {
    __asm__ __volatile__ ( "mov cr3, rax" : : "a"(pagetable_phys) : "memory" );
}

ALWAYS_INLINE void tlb_flush_all(void) {
    __asm__ __volatile__ ( "mov rax, cr3 \t\n mov cr3, rax" : : : "rax", "memory" );
}

ALWAYS_INLINE void tlb_flush_for(void* virt) {
    __asm__ __volatile__ ( "invlpg [%0]" : : "r"(virt) : "memory" );
}

ALWAYS_INLINE void load_gdt(void* gdt, size_t size) {
    struct {
        uint16_t size;
        void* base;
    } __attribute__((packed)) gdtr = { (uint16_t)(size - 1), gdt };
    __asm__ __volatile__ ( "lgdt [%0]" : : "m"(gdtr) );
}

ALWAYS_INLINE void load_idt(void* idt, size_t size) {
    struct {
        uint16_t size;
        void* base;
    } __attribute__((packed)) idtr = { (uint16_t)(size - 1), idt };
    __asm__ __volatile__ ( "lidt [%0]" : : "m"(idtr) );
}

ALWAYS_INLINE void load_tss(uint16_t selector) {
    __asm__ __volatile__ ( "ltr %0" : : "r"(selector) );
}
