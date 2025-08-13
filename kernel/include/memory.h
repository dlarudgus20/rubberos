#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <freec/inttypes.h>
#include <buddy/slice.h>
#include <slab/slab.h>
#include <collections/arraylist.h>

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

extern struct slab_page_allocator g_slab_page_allocator;
#define SLAB_INIT(slab, type) slab_init(slab, sizeof(type), alignof(type), &g_slab_page_allocator)

extern struct arraylist_allocator g_arraylist_allocator;

void memory_init(void);

volatile void* mmio_alloc_mapping(uintptr_t begin_phys, uintptr_t end_phys);
void mmio_dealloc_mapping(uintptr_t begin_virt, uintptr_t end_virt);

struct slice dynmem_alloc(size_t len);
void dynmem_dealloc(void* ptr, size_t len);

void mmap_print_bootinfo(void);
void mmap_print_dyn(void);
void pagetable_print(void);
void dynmem_print(void);

void dynmem_test_seq(void);

#include "arch/memory.h"
