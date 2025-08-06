#pragma once

#include <stdint.h>

//#define MMAP_ENTRY_AVAILABLE 1    // defined in arch-independent memory.h
#define MMAP_ENTRY_RESERVED 2
#define MMAP_ENTRY_ACPI_RECLAIMABLE 3
#define MMAP_ENTRY_ACPI_NVS 4
#define MMAP_ENTRY_BADRAM 5

#define PAGETABLE_LENGTH 512

typedef uint64_t page_entry_t;
typedef page_entry_t pagetable_t[PAGETABLE_LENGTH];

#define PAGE_SIZE 0x1000

#define DYNMEM_START_PHYS_MINIMUM   0x00800000
#define DYNMEM_START_VIRT           0x00200000

#define KERNEL_SIZE             0x00400000
#define KERNEL_START_PHYS       0x00200000
#define KERNEL_START_VIRT       0xffffffff80000000

#define KSTACK_SIZE             0x00200000
#define KSTACK_START_VIRT       0xffffffff8f000000

#define IOMAP_VIRT_SIZE         0x0000007f00000000
#define IOMAP_START_VIRT        0xffffff8000000000

#define PAGE_FLAG_PRESENT       (1ul << 0)
#define PAGE_FLAG_WRITABLE      (1ul << 1)
#define PAGE_FLAG_USER          (1ul << 2)
#define PAGE_FLAG_WRITE_THROUGH (1ul << 3)
#define PAGE_FLAG_NO_CACHE      (1ul << 4)
#define PAGE_FLAG_ACCESSED      (1ul << 5)
#define PAGE_FLAG_DIRTY         (1ul << 6)
#define PAGE_FLAG_HUGE          (1ul << 7)
#define PAGE_FLAG_GLOBAL        (1ul << 8)
#define PAGE_FLAG_NO_EXECUTE    (1ul << 63)

#define PAGE_MASK_ADDR          0x000ffffffffff000

#define PAGE_FLAG_NIL 0
#define KERNEL_PAGE_FLAG (PAGE_FLAG_PRESENT | PAGE_FLAG_WRITABLE)

struct mmap;

void pagetable_construct(const struct mmap* mmap_dyn);

// all addresses must be aligned by PAGE_SIZE
// if phys == 0, chagne only flags.
void pagetable_map(uintptr_t begin_virt, uintptr_t end_virt, uintptr_t phys, page_entry_t flags);

void pagetable_print_with_dyn(const struct mmap* mmap_dyn);
