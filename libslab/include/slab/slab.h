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
    size_t object_size;
    size_t object_align;
};

void slab_init(struct slab_allocator* slab, size_t object_size, size_t object_align, const struct slab_page_allocator* pa);
void* slab_alloc(struct slab_allocator* slab);
void slab_dealloc(struct slab_allocator* slab, void* ptr);
