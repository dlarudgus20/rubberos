#pragma once

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
void mmap_print_bootinfo(void);
void mmap_print_dyn(void);

// arch
const char* mmap_entry_type_str(mmap_entry_type type);
void mem_pagetable_print(void);
