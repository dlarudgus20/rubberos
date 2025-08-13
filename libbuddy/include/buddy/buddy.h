#pragma once

#include <stddef.h>
#include <stdint.h>

#include "slice.h"

#define BUDDY_UNIT 4096

struct buddy_blocks {
    uintptr_t start_addr;
    size_t total_len;
    size_t metadata_len;
    size_t data_offset;
    size_t units;
    uint32_t levels;
    size_t used;
    void* bitmaps;
    size_t bitmaps_len;
};

void buddy_init(struct buddy_blocks *buddy, void* start_addr, size_t len);
struct slice buddy_alloc_slice(struct buddy_blocks* buddy, size_t len);
void buddy_dealloc(struct buddy_blocks* buddy, void* addr, size_t len);

#define buddy_alloc(buddy, len) (buddy_alloc_slice(buddy, len).ptr)
