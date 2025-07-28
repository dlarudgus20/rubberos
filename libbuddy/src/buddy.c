#include "buddy/buddy.h"
#include <stdbool.h>
#include <freec/string.h>
#include <freec/assert.h>
#include <freec/stdlib.h>

struct block_bitmap {
    uint8_t* bits;
    size_t count;
};

static void buddy_info_calc_metadata(struct buddy_blocks* buddy, size_t data_len) {
    assert(data_len > BUDDY_UNIT);

    uint32_t levels = 0;
    uint32_t bits = 0;
    const size_t units = (data_len - 1) / BUDDY_UNIT + 1;
    size_t block_count = units;

    while (1) {
        levels++;
        bits += (block_count / 8) + 1;

        if (block_count == 1) {
            break;
        }
        block_count /= 2;
    }

    const size_t metadata_len = levels * sizeof(struct block_bitmap) + bits;
    assert(metadata_len < data_len);

    // start_addr, total_len, data_offset are not decided
    buddy->metadata_len = metadata_len;
    buddy->units = units;
    buddy->levels = levels;
}

static void buddy_info_init(struct buddy_blocks* buddy, uintptr_t start_addr, size_t total_len) {
    buddy_info_calc_metadata(buddy, total_len);
    const size_t data_offset = szdiv_ceil(buddy->metadata_len, BUDDY_UNIT) * BUDDY_UNIT;
    buddy_info_calc_metadata(buddy, total_len - data_offset);
    assert(buddy->metadata_len < data_offset);

    buddy->start_addr = start_addr;
    buddy->total_len = total_len;
    buddy->data_offset = data_offset;
}

void buddy_init(struct buddy_blocks* buddy, uintptr_t start_addr, size_t total_len) {
    buddy_info_init(buddy, start_addr, total_len);

    const size_t bitmaps_len = buddy->levels;
    const size_t bitmaps_bytes = bitmaps_len * sizeof(struct block_bitmap);
    struct block_bitmap* const bitmaps = (struct block_bitmap*)start_addr;

    const size_t total_bits_len = buddy->metadata_len - bitmaps_bytes;
    uint8_t* const total_bits = (uint8_t*)(start_addr + bitmaps_bytes);

    memset(total_bits, 0, total_bits_len);

    size_t block_count = buddy->units;
    size_t bits_idx = 0;
    size_t idx = 0;
    do {
        const size_t bits_len = (block_count - 1) / 8 + 1;

        size_t count = 0;
        if (block_count % 2 != 0) {
            total_bits[bits_idx + bits_len - 1] = 1 << (block_count % 8 - 1);
            count = 1;
        }

        bitmaps[idx] = (struct block_bitmap){ .bits = total_bits + bits_idx, .count = count };

        bits_idx += bits_len;
        idx += 1;
        block_count /= 2;
    } while (block_count != 0);

    assert(bits_idx == total_bits_len);
    assert(idx == bitmaps_len);

    buddy->used = 0;
    buddy->bitmaps = bitmaps;
    buddy->bitmaps_len = bitmaps_len;
}

static size_t bitmap_index_for_size(size_t size) {
    size_t idx = 0;
    for (; ((size_t)BUDDY_UNIT << idx) < size; idx++) {}
    return idx;
}

static bool is_empty(struct block_bitmap* bitmap) {
    return bitmap->count == 0;
}

static bool get_bit(struct block_bitmap* bitmap, size_t block_index) {
    return (bitmap->bits[block_index / 8] & (1 << (block_index % 8))) != 0;
}

static void set_1(struct block_bitmap* bitmap, size_t block_index) {
    const bool prev = get_bit(bitmap, block_index);
    bitmap->bits[block_index / 8] |= 1 << (block_index % 8);
    if (!prev) {
        bitmap->count += 1;
    }
}

static void set_0(struct block_bitmap* bitmap, size_t block_index) {
    const bool prev = get_bit(bitmap, block_index);
    bitmap->bits[block_index / 8] &= ~(1 << (block_index % 8));
    if (prev) {
        bitmap->count -= 1;
    }
}

static size_t get_first_1(struct block_bitmap* bitmap) {
    assert(!is_empty(bitmap));

    size_t bits_idx = 0;
    for (; bitmap->bits[bits_idx] == 0; bits_idx++) {}

    size_t offset = 0;
    for (; offset < 8 && (bitmap->bits[bits_idx] & (1 << offset)) == 0; offset++) {}
    assert(offset < 8);

    return bits_idx * 8 + offset;
}

uintptr_t buddy_alloc(struct buddy_blocks* buddy, size_t len) {
    assert(len != 0);

    const size_t aligned_len = szdiv_ceil(len, BUDDY_UNIT) * BUDDY_UNIT;
    const size_t bitmap_idx_fit = bitmap_index_for_size(aligned_len);
    const size_t bitmaps_len = buddy->bitmaps_len;

    if (bitmap_idx_fit >= bitmaps_len) {
        // requested memory is too large
        return 0;
    }

    for (size_t bitmap_idx = bitmap_idx_fit; bitmap_idx <= bitmaps_len; bitmap_idx++) {
        struct block_bitmap* const bitmap = ((struct block_bitmap*)buddy->bitmaps) + bitmap_idx;

        if (is_empty(bitmap)) {
            continue;
        }

        const size_t block_index = get_first_1(bitmap);
        set_0(bitmap, block_index);

        size_t below_block_index = block_index;
        for (size_t below = bitmap_idx; below-- > bitmap_idx_fit; ) {
            below_block_index *= 2;
            struct block_bitmap* const below_bitmap = ((struct block_bitmap*)buddy->bitmaps) + below;
            set_1(below_bitmap, below_block_index + 1);
        }

        buddy->used += aligned_len;

        const uintptr_t data_addr = buddy->start_addr + buddy->data_offset;
        return data_addr + block_index * (BUDDY_UNIT << bitmap_idx);
    }

    // there is no memory to allocate
    return 0;
}

void buddy_dealloc(struct buddy_blocks* buddy, uintptr_t addr, size_t len) {
    if (len == 0) {
        return;
    }

    const uintptr_t data_addr = buddy->start_addr + buddy->data_offset;
    const size_t data_len = buddy->total_len - buddy->data_offset;

    const uintptr_t aligned_addr = addr / BUDDY_UNIT * BUDDY_UNIT;
    const uintptr_t aligned_end = uptrdiv_ceil(addr + len, BUDDY_UNIT) * BUDDY_UNIT;
    const size_t aligned_len = aligned_end - aligned_addr;

    assert(data_addr <= aligned_addr && aligned_addr < data_addr + data_len);
    assert(data_addr < aligned_end && aligned_end <= data_addr + data_len);

    const size_t bitmap_idx_fit = bitmap_index_for_size(aligned_len);
    const size_t bitmaps_len = buddy->bitmaps_len;

    assert(bitmap_idx_fit < bitmaps_len);

    size_t block_index = (aligned_addr - data_addr) / (BUDDY_UNIT << bitmap_idx_fit);
    size_t current = bitmap_idx_fit;
    while (1) {
        struct block_bitmap* const bitmap = ((struct block_bitmap*)buddy->bitmaps) + current;

        assert(!get_bit(bitmap, block_index));
        set_1(bitmap, block_index);

        const size_t buddy_index = block_index ^ 1;
        if (!get_bit(bitmap, buddy_index)) {
            break;
        }
        if (current + 1 >= bitmaps_len) {
            break;
        }

        set_0(bitmap, buddy_index);
        set_0(bitmap, block_index);

        block_index /= 2;
        current += 1;
    }

    buddy->used -= aligned_len;
}
