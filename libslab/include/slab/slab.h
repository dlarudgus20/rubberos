#pragma once

#include <stddef.h>
#include <stdint.h>

#include <collections/linkedlist.h>

#define SLAB_PAGE 4096

struct slab_page_allocator {
    void* ctx;
    void* (*alloc)(void* ctx);
    void (*dealloc)(void* ctx, void* page);
};

struct slab_allocator {
    struct slab_page_allocator page_allocator;
    struct linkedlist partial_list;
    uint16_t object_size;
    uint16_t object_align;
    uint16_t payload_offset;
    uint16_t slot_size;
};

size_t slab_page_offset(size_t align);
size_t slab_slot_header_size(void);
size_t slab_redzone_size(void);

void slab_init(struct slab_allocator* slab, size_t size, size_t align, const struct slab_page_allocator* pa);
void* slab_alloc(struct slab_allocator* slab);
void slab_dealloc(struct slab_allocator* slab, void* ptr);
