#include <iostream>
#include <gtest/gtest.h>

extern "C" {
#define restrict
#include "buddy/buddy.h"
};

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <vector>

struct alignas(4096) page {
    uint64_t buf[512];
};

struct test_buddy {
    buddy_blocks buddy;
    std::vector<page> mem;
    test_buddy() {
        mem.resize(0x00200000 / 4096);
        buddy_init(&buddy, (uintptr_t)mem.data(), mem.size() * sizeof(page));
    }
    buddy_blocks* get() {
        return &buddy;
    }
    buddy_blocks* operator ->() {
        return &buddy;
    }
};

TEST(buddy_test, create) {
    test_buddy buddy;

    const uintptr_t begin = (uintptr_t)buddy.mem.data();
    const size_t len = buddy.mem.size() * sizeof(page);

    const size_t units = 0x1ff000 / BUDDY_UNIT;
    size_t level = 0;
    size_t bitlen = 0;
    for (; units >> level; level++) {
        bitlen += ((units >> level) + 7) / 8;
    }
    const size_t metalen = level * 16 + bitlen;

    ASSERT_EQ(buddy->start_addr, begin);
    ASSERT_EQ(buddy->total_len, len);
    ASSERT_EQ(buddy->metadata_len, metalen);
    ASSERT_EQ(buddy->data_offset, BUDDY_UNIT);
    ASSERT_EQ(buddy->units, units);
    ASSERT_EQ(buddy->levels, level);
}

TEST(buddy_test, seq) {
    test_buddy buddy;

    for (size_t level = 0; level < buddy->levels; level++) {
        const size_t block_count = buddy->units >> level;
        const size_t size = BUDDY_UNIT << level;

        ASSERT_EQ(buddy->used, 0);

        for (size_t index = 0; index < block_count; index++) {
            if (const uintptr_t addr = buddy_alloc(buddy.get(), size - 1)) {
                volatile uint32_t* const slice = (uint32_t*)addr;
                const size_t count = size / 4;
                for (size_t i = 0; i < count; i++) {
                    slice[i] = (uint32_t)i;
                }
                for (size_t i = 0; i < count; i++) {
                    ASSERT_EQ(slice[i], (uint32_t)i);
                }
            }
        }

        ASSERT_EQ(buddy->used, (buddy->total_len - buddy->data_offset) / size * size);

        for (size_t index = 0; index < block_count; index++) {
            const uintptr_t addr = buddy->start_addr + buddy->data_offset + size * index;
            buddy_dealloc(buddy.get(), addr + 1, size - 1);
        }

        ASSERT_EQ(buddy->used, 0);
    }
}
