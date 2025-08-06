#pragma once

#include <stddef.h>
#include <stdint.h>
#include <freec/inttypes.h>

typedef uint64_t mmap_ulong;
typedef uint32_t mmap_entry_type;

#define PRImul PRIx64
#define PRImet PRIu32

#define MMAP_ENTRY_AVAILABLE 1
// arch/memory.h for other flags

struct mmap_entry {
    mmap_ulong base;
    mmap_ulong length;
    mmap_entry_type type;
};

struct mmap {
    uint32_t len;
    uint32_t reserved;
    struct mmap_entry entries[];
};

void mmap_init(void);

uintptr_t mmio_alloc_mapping(uintptr_t begin_phys, uintptr_t end_phys);
void mmio_dealloc_mapping(uintptr_t begin_virt, uintptr_t end_virt);

void* dynmem_alloc_page(size_t len);
void dynmem_dealloc_page(void* ptr, size_t len);

void mmap_print_bootinfo(void);
void mmap_print_dyn(void);
void pagetable_print(void);
void dynmem_print(void);

void dynmem_test_seq(void);

// arch
const char* mmap_entry_type_str(mmap_entry_type type);
