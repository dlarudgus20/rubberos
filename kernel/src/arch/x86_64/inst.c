#include "arch/x86_64/inst.h"

extern inline void out8(uint16_t port, uint8_t data);
extern inline uint8_t in8(uint16_t port);
extern inline void wait_for_int(void);
extern inline void pagetable_set(uintptr_t pagetable_phys);
extern inline void tlb_flush_all(void);
extern inline void tlb_flush_for(void* virt);
