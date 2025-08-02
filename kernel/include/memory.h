#pragma once

#include <stdint.h>

typedef uint64_t mmap_ulong;
typedef uint32_t mmap_entry_type;

#define PRImul "lx"
#define PRImet "u"

#define MMAP_ENTRY_AVAILABLE 1
#define MMAP_ENTRY_RESERVED 2
#define MMAP_ENTRY_ACPI_RECLAIMABLE 3
#define MMAP_ENTRY_ACPI_NVS 4
#define MMAP_ENTRY_BADRAM 5

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
