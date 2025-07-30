#include <iostream>
#include <gtest/gtest.h>

extern "C" {
#define restrict
#include "slab/slab.h"
};

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <random>
#include <stdexcept>
#include <utility>
#include <functional>
#include <memory>

template <typename T>
T swap_remove(std::vector<T>& v, size_t index) {
    using std::swap;
    T x = std::move(v[index]);
    swap(v[index], v.back());
    v.pop_back();
    return x;
}

extern "C" void* test_alloc(void* ctx);
extern "C" void test_dealloc(void* ctx, void* ptr);

struct test_slab {
    slab_page_allocator pa;
    slab_allocator sa;

    int count = 0;
    bool print_logs = false;
    std::vector<std::pair<void*, int>> pages;
    std::vector<std::pair<void*, int>> deallocated;

    std::function<void(test_slab&)> on_after_alloc;
    std::function<void(test_slab&)> on_before_dealloc;

    test_slab(const test_slab&) = delete;
    test_slab& operator =(const test_slab&) = delete;

    test_slab(size_t size, size_t align, bool logs = false)
        : print_logs(logs) {
        pa.ctx = this;
        pa.alloc = test_alloc;
        pa.dealloc = test_dealloc;
        slab_init(&sa, size, align, &pa);
    }
    ~test_slab() {
        for (auto [p, i] : deallocated) {
            bool ok = std::all_of((char*)p, (char*)p + SLAB_PAGE, [](char x) {
                return (unsigned char)x == 0xdd;
            });
            if (!ok) {
                abort();
            }
            free(p);
        }
        for (auto [p, i] : pages) {
            free(p);
        }
    }
    slab_allocator* operator ->() {
        return &sa;
    }
    slab_allocator* get() {
        return &sa;
    }
};

extern "C" void* test_alloc(void* ctx) {
    auto self = static_cast<test_slab*>(ctx);
    void* const page = aligned_alloc(SLAB_PAGE, SLAB_PAGE);
    memset(page, 0xcc, SLAB_PAGE);
    self->pages.emplace_back(page, ++self->count);
    if (self->print_logs) {
        printf("page #%d allocated\n", self->count);
    }
    if (self->on_after_alloc) {
        self->on_after_alloc(*self);
    }
    return page;
}

extern "C" void test_dealloc(void* ctx, void* ptr) {
    auto self = static_cast<test_slab*>(ctx);
    auto it = find_if(self->pages.begin(), self->pages.end(), [ptr](auto pr) {
        return pr.first == ptr;
    });
    ASSERT_TRUE(it != self->pages.end());
    if (self->on_before_dealloc) {
        self->on_before_dealloc(*self);
    }
    memset(it->first, 0xdd, SLAB_PAGE);
    self->deallocated.push_back(*it);
    if (self->print_logs) {
        printf("page #%d deallocated\n", self->count);
    }
    self->pages.erase(it);
}

TEST(slab_test, creation) {
    test_slab slab(64, 8);
    ASSERT_EQ(slab->object_size, 64);
    ASSERT_EQ(slab->object_align, 8);
    ASSERT_EQ(slab->payload_offset, 24);
    ASSERT_EQ(slab->slot_size, 104);
    ASSERT_TRUE(linkedlist_is_empty(&slab->partial_list));
}

TEST(slab_test, dealloc_once) {
    test_slab slab(64, 8);
    void* p = slab_alloc(slab.get());
    ASSERT_TRUE(p);
    slab_dealloc(slab.get(), p);
}

TEST(slab_test, alignment_8) {
    test_slab slab(24, 8);
    void* p = slab_alloc(slab.get());
    ASSERT_TRUE(p);
    ASSERT_EQ((uintptr_t)p % 8, 0);
    slab_dealloc(slab.get(), p);
}

TEST(slab_test, alignment_16) {
    test_slab slab(32, 16);
    void* p = slab_alloc(slab.get());
    ASSERT_TRUE(p);
    ASSERT_EQ((uintptr_t)p % 16, 0);
    slab_dealloc(slab.get(), p);
}


TEST(slab_test, alignment_64) {
    test_slab slab(128, 64);
    void* p = slab_alloc(slab.get());
    ASSERT_TRUE(p);
    ASSERT_EQ((uintptr_t)p % 64, 0);
    slab_dealloc(slab.get(), p);
}

TEST(slab_test, multiple_chunks) {
    test_slab slab(64, 32);

    std::vector<void*> ptrs;
    for (int i = 0; i < 10; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ASSERT_EQ((uintptr_t)ptr % 32, 0);
        ptrs.push_back(ptr);
    }

    for (auto p : ptrs) {
        slab_dealloc(slab.get(), p);
    }
}

TEST(slab_test, random_sizes) {
    test_slab slab(200, 128);

    std::vector<void*> ptrs;
    for (int i = 0; i < 5; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ASSERT_EQ((uintptr_t)ptr % 128, 0);
        ptrs.push_back(ptr);
    }

    for (auto p : ptrs) {
        slab_dealloc(slab.get(), p);
    }
}

TEST(slab_test, exhaustion_and_reuse) {
    test_slab slab(32, 16);

    size_t page_offset = slab_page_offset(slab->object_align);
    ASSERT_EQ(page_offset, 32);
    ASSERT_EQ(slab->slot_size, 80);
    size_t chunks_per_page = (SLAB_PAGE - page_offset) / slab->slot_size;
    std::vector<void*> ptrs;

    // Allocate all possible chunks in a page
    for (size_t i = 0; i < chunks_per_page; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }

    // Allocating one more should allocate a new page
    ASSERT_EQ(slab.pages.size(), 1);
    void* extra = slab_alloc(slab.get());
    ASSERT_TRUE(extra);
    ASSERT_EQ(slab.pages.size(), 2);

    // Deallocate all and ensure reuse
    for (auto ptr : ptrs) {
        //ASSERT_EQ(slab.pages.size(), 2);
        slab_dealloc(slab.get(), ptr);
    }
    ASSERT_EQ(slab.pages.size(), 1);
    slab_dealloc(slab.get(), extra);
    ASSERT_EQ(slab.pages.size(), 0);

    // After deallocation, allocation should succeed again
    void* ptr = slab_alloc(slab.get());
    ASSERT_EQ(slab.pages.size(), 1);
    ASSERT_TRUE(ptr);
    slab_dealloc(slab.get(), ptr);
    ASSERT_EQ(slab.pages.size(), 0);
}

TEST(slab_test, page_boundary_dealloc) {
    test_slab slab(40, 8);

    size_t page_offset = slab_page_offset(slab->object_align);
    ASSERT_EQ(page_offset, 24);
    ASSERT_EQ(slab->slot_size, 80);
    size_t chunks_per_page = (SLAB_PAGE - page_offset) / slab->slot_size;

    std::vector<void*> ptrs;
    for (size_t i = 0; i < chunks_per_page; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }

    ASSERT_EQ(slab.pages.size(), 1);
    void* extra1 = slab_alloc(slab.get());
    ASSERT_TRUE(extra1);
    ASSERT_EQ(slab.pages.size(), 2);
    void* extra2 = slab_alloc(slab.get());
    ASSERT_TRUE(extra2);
    ASSERT_EQ(slab.pages.size(), 2);

    slab_dealloc(slab.get(), extra1);
    ASSERT_EQ(slab.pages.size(), 2);
    slab_dealloc(slab.get(), extra2);
    ASSERT_EQ(slab.pages.size(), 1);

    for (auto p : ptrs) {
        slab_dealloc(slab.get(), p);
    }
}

TEST(slab_test, random_alloc_dealloc_pattern) {
    test_slab slab(40, 8);

    unsigned seed = std::random_device()();
    std::mt19937 rng(seed);
    std::cout << "seed: " << seed << "\n";

    std::vector<void*> ptrs;

    // Randomly allocate and deallocate
    for (int i = 0; i < 100; i++) {
        std::bernoulli_distribution dist_if(0.6);
        if (dist_if(rng) || ptrs.empty()) {
            void* ptr = slab_alloc(slab.get());
            ASSERT_TRUE(ptr);
            ptrs.push_back(ptr);
        } else {
            std::uniform_int_distribution<size_t> dist_idx(0, ptrs.size() - 1);
            size_t idx = dist_idx(rng);
            void* ptr = swap_remove(ptrs, idx);
            slab_dealloc(slab.get(), ptr);
        }
    }

    // Clean up
    for (void* ptr : ptrs) {
        slab_dealloc(slab.get(), ptr);
    }
}

extern "C" void* failing_alloc(void* ctx) {
    return NULL;
}

extern "C" void failing_dealloc(void* ctx, void* ptr) {
    abort();
}

TEST(slab_test, null_alloc_returns_null) {
    slab_page_allocator pa;
    pa.ctx = NULL;
    pa.alloc = failing_alloc;
    pa.dealloc = failing_dealloc;
    slab_allocator sa;
    slab_init(&sa, 64, 8, &pa);
    void *ptr = slab_alloc(&sa);
    ASSERT_FALSE(ptr);
}

TEST(slab_test, large_chunk) {
    test_slab slab(SLAB_PAGE / 2, 8);

    void* ptr1 = slab_alloc(slab.get());
    void* ptr2 = slab_alloc(slab.get());
    void* ptr3 = slab_alloc(slab.get());
    ASSERT_TRUE(ptr1);
    ASSERT_TRUE(ptr2);
    ASSERT_TRUE(ptr3);

    slab_dealloc(slab.get(), ptr1);
    slab_dealloc(slab.get(), ptr2);
    slab_dealloc(slab.get(), ptr3);
}

TEST(slab_test, interleaved_alloc_dealloc_multiple_types) {
    test_slab slab1(48, 16);
    test_slab slab2(64, 32);

    std::vector<void*> ptrs_a, ptrs_b;

    // Interleaved allocation
    for (int i = 0; i < 20; i++) {
        if (i % 2 == 0) {
            void* ptr = slab_alloc(slab1.get());
            ASSERT_TRUE(ptr);
            ptrs_a.push_back(ptr);
        } else {
            void* ptr = slab_alloc(slab2.get());
            ASSERT_TRUE(ptr);
            ptrs_b.push_back(ptr);
        }
    }

    // Interleaved deallocation
    for (int i = 0; i < 20; i++) {
        if (i % 2 == 0) {
            slab_dealloc(slab1.get(), ptrs_a[i / 2]);
        } else {
            slab_dealloc(slab2.get(), ptrs_b[i / 2]);
        }
    }
}

TEST(slab_test, stress_many_pages) {
    test_slab slab(32, 8);

    size_t page_offset = slab_page_offset(slab->object_align);
    ASSERT_EQ(page_offset, 24);
    ASSERT_EQ(slab->slot_size, 72);
    const size_t chunks_per_page = (SLAB_PAGE - page_offset) / slab->slot_size;
    const size_t total_chunks = chunks_per_page * 50;

    std::vector<void*> ptrs;
    for (size_t i = 0; i < total_chunks; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }
    for (void* ptr : ptrs) {
        slab_dealloc(slab.get(), ptr);
    }
}

TEST(slab_test, repeated_alloc_dealloc_cycles) {
    test_slab slab(32, 8);

    unsigned seed = std::random_device()();
    std::mt19937 rng(seed);
    std::cout << "seed: " << seed << "\n";

    const size_t test_chunks = SLAB_PAGE / 32 * 3;
    for (int i = 0; i < 10; i++) {
        std::vector<void*> ptrs;
        for (size_t i = 0; i < test_chunks; i++) {
            void* ptr = slab_alloc(slab.get());
            ASSERT_TRUE(ptr);
            ptrs.push_back(ptr);
        }
        shuffle(ptrs.begin(), ptrs.end(), rng);
        for (void* ptr : ptrs) {
            slab_dealloc(slab.get(), ptr);
        }
    }
}

TEST(slab_test, double_free_panics) {
    test_slab slab(32, 8);
    void* ptr = slab_alloc(slab.get());
    ASSERT_TRUE(ptr);
    slab_dealloc(slab.get(), ptr);
    ASSERT_DEATH(slab_dealloc(slab.get(), ptr), "try to deallocate an object that is not allocated");
}

TEST(slab_test, alloc_dealloc_pattern_with_gaps) {
    test_slab slab(24, 8);

    std::vector<void*> ptrs, new_ptrs;
    for (int i = 0; i < 30; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }
    // Deallocate chunks stepping by 3
    for (size_t i = 0; i < ptrs.size(); i += 3) {
        slab_dealloc(slab.get(), ptrs[i]);
        ptrs[i] = 0;
    }
    // Allocate again, should reuse freed slots
    for (int i = 0; i < 10; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        new_ptrs.push_back(ptr);
    }
    // Clean up
    for (size_t i = 0; i < ptrs.size(); i++) {
        if (i % 3 != 0) {
            slab_dealloc(slab.get(), ptrs[i]);
        } else {
            ASSERT_FALSE(ptrs[i]);
        }
    }
    for (auto ptr : new_ptrs) {
        slab_dealloc(slab.get(), ptr);
    }
}

TEST(slab_test, slab_fragmentation_and_reuse) {
    test_slab slab(40, 8);

    std::vector<void*> ptrs, reused;
    for (int i = 0; i < 100; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }
    // Free every even-indexed chunk
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        slab_dealloc(slab.get(), ptrs[i]);
    }
    // Allocate again, should fill freed slots
    size_t page_count = slab.pages.size();
    for (int i = 0; i < 50; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        reused.push_back(ptr);
    }
    ASSERT_EQ(slab.pages.size(), page_count);
    // Clean up
    for (size_t i = 1; i < ptrs.size(); i += 2) {
        slab_dealloc(slab.get(), ptrs[i]);
    }
    for (auto ptr : reused) {
        slab_dealloc(slab.get(), ptr);
    }
}

TEST(slab_test_old, multiple_pages_allocation_and_deallocation) {
    test_slab slab(64, 8);

    std::vector<void*> ptrs;

    // Allocate enough objects to span multiple pages
    for (int i = 0; i < SLAB_PAGE / 64 * 3; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }

    // Ensure all pointers are unique
    for (size_t i = 0; i < ptrs.size(); i++) {
        for (size_t j = i + 1; j < ptrs.size(); j++) {
            ASSERT_NE(ptrs[i], ptrs[j]);
        }
    }

    // Deallocate all objects
    for (auto ptr : ptrs) {
        slab_dealloc(slab.get(), ptr);
    }

    // Ensure allocator can reuse pages after deallocation
    ASSERT_TRUE(slab_alloc(slab.get()));
}

TEST(slab_test_old, random_allocation_and_deallocation_sequencial) {
    test_slab slab(64, 8);

    unsigned seed = std::random_device()();
    std::mt19937 rng(seed);
    std::cout << "seed: " << seed << "\n";

    std::vector<void*> ptrs;

    // Allocate enough objects to exceed 3 times the page size
    for (size_t i = 0; i < SLAB_PAGE / 64 * 4; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }

    // Shuffle the allocated pointers to randomize deallocation order
    shuffle(ptrs.begin(), ptrs.end(), rng);

    // Deallocate in random order
    for (auto ptr : ptrs) {
        slab_dealloc(slab.get(), ptr);
    }

    // Ensure allocator can reuse pages after deallocation
    ASSERT_TRUE(slab_alloc(slab.get()));
}

TEST(slab_test_old, random_allocation_and_deallocation_interleaved) {
    test_slab slab(64, 8);

    unsigned seed = std::random_device()();
    std::mt19937 rng(seed);
    std::cout << "seed: " << seed << "\n";

    std::vector<void*> ptrs;

    // Interleave allocation and deallocation
    size_t repeat = SLAB_PAGE / 64 * 4;
    for (size_t i = 0; i < repeat; i++) {
        std::bernoulli_distribution dist_if((double)i / repeat);
        if (dist_if(rng) && !ptrs.empty()) {
            std::uniform_int_distribution<size_t> dist_idx(0, ptrs.size() - 1);
            size_t index = dist_idx(rng);
            void* ptr = swap_remove(ptrs, index);
            slab_dealloc(slab.get(), ptr);
        } else {
            void* ptr = slab_alloc(slab.get());
            ASSERT_TRUE(ptr);
            ptrs.push_back(ptr);
        }
    }

    // Deallocate left pointers
    for (auto ptr : ptrs) {
        slab_dealloc(slab.get(), ptr);
    }

    // Ensure allocator can still allocate after random deallocation
    ASSERT_TRUE(slab_alloc(slab.get()));
}

TEST(slab_test_old, large_allocation_and_deallocation) {
    test_slab slab(128, 16);

    std::vector<void*> ptrs;

    // Allocate enough objects to span multiple pages
    for (size_t i = 0; i < SLAB_PAGE / 128 * 5; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }

    // Ensure all pointers are unique
    for (size_t i = 0; i < ptrs.size(); i++) {
        for (size_t j = i + 1; j < ptrs.size(); j++) {
            ASSERT_NE(ptrs[i], ptrs[j]);
        }
    }

    // Deallocate all objects
    for (auto ptr : ptrs) {
        slab_dealloc(slab.get(), ptr);
    }

    // Ensure allocator can reuse pages after deallocation
    ASSERT_TRUE(slab_alloc(slab.get()));
}

TEST(slab_test_old, interleaved_allocation_and_deallocation) {
    test_slab slab(64, 8);

    std::vector<void*> ptrs;

    // Interleave allocation and deallocation
    for (int i = 0; i < 100; i++) {
        if (i % 3 == 0 && !ptrs.empty()) {
            void* ptr = ptrs.back();
            ptrs.pop_back();
            slab_dealloc(slab.get(), ptr);
        } else {
            void* ptr = slab_alloc(slab.get());
            ASSERT_TRUE(ptr);
            ptrs.push_back(ptr);
        }
    }

    // Deallocate remaining objects
    for (auto ptr : ptrs) {
        slab_dealloc(slab.get(), ptr);
    }

    // Ensure allocator can still allocate after interleaved operations
    ASSERT_TRUE(slab_alloc(slab.get()));
}

TEST(slab_test_old, fragmentation_handling) {
    test_slab slab(128, 16);

    std::vector<void*> ptrs;

    // Allocate and deallocate in a pattern to create fragmentation
    for (int i = 0; i < 50; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        if (i % 2 == 0) {
            slab_dealloc(slab.get(), ptr);
        } else {
            ptrs.push_back(ptr);
        }
    }

    // Deallocate remaining objects
    for (auto ptr : ptrs) {
        slab_dealloc(slab.get(), ptr);
    }

    // Ensure allocator can still allocate after fragmentation
    ASSERT_TRUE(slab_alloc(slab.get()));
}

TEST(slab_test_old, stress_allocation_and_deallocation) {
    test_slab slab(256, 32);

    std::vector<void*> ptrs;

    // Stress test with a large number of allocations and deallocations
    for (int i = 0; i < 1000; i++) {
        if (i % 5 == 0 && !ptrs.empty()) {
            void* ptr = ptrs.back();
            ptrs.pop_back();
            slab_dealloc(slab.get(), ptr);
        } else {
            void* ptr = slab_alloc(slab.get());
            ASSERT_TRUE(ptr);
            ptrs.push_back(ptr);
        }
    }

    // Deallocate remaining objects
    for (auto ptr : ptrs) {
        slab_dealloc(slab.get(), ptr);
    }

    // Ensure allocator can still allocate after stress test
    ASSERT_TRUE(slab_alloc(slab.get()));
}

TEST(slab_test, object_too_large_for_page) {
    size_t minimal_metadata = slab_page_offset(1) + slab_slot_header_size() + slab_redzone_size() * 2;
    test_slab slab(SLAB_PAGE - minimal_metadata, 1);
    ASSERT_DEATH({
        test_slab slab(SLAB_PAGE - minimal_metadata + 1, 1);
    }, "object is too big");
}

TEST(slab_test, object_too_large_for_page_by_align) {
    size_t boundary = 128 * 2 + slab_redzone_size();
    test_slab slab(SLAB_PAGE - boundary, 128);
    ASSERT_DEATH({
        test_slab slab(SLAB_PAGE - boundary + 1, 128);
    }, "object is too big");
}

TEST(slab_test, redzone_detection_on_overflow) {
    test_slab slab(32, 8);

    char* ptr = (char*)slab_alloc(slab.get());
    ASSERT_TRUE(ptr);

    // Write past the end of the chunk to trigger redzone detection.
    // The redzone is assumed to be immediately after the chunk.
    ASSERT_DEATH({
        // Write 1 byte past the end of the chunk
        ptr[32] = '\xaa';
        slab_dealloc(slab.get(), ptr);
    }, "redzone is corrupted");
}

TEST(slab_test, redzone_detection_on_underflow) {
    test_slab slab(32, 8);

    char* ptr = (char*)slab_alloc(slab.get());
    ASSERT_TRUE(ptr);

    ASSERT_DEATH({
        // Write 1 byte before the start of the chunk
        ptr[-1] = '\xbb';
        slab_dealloc(slab.get(), ptr);
    }, "redzone is corrupted");
}

TEST(slab_test, redzone_integrity_on_normal_use) {
    test_slab slab(32, 8);

    char* ptr = (char*)slab_alloc(slab.get());
    ASSERT_TRUE(ptr);

    // Write within the chunk boundaries
    for (int i = 0; i < 32; i++) {
        ptr[i] = i;
    }
    slab_dealloc(slab.get(), ptr);
}

TEST(slab_test, alloc_dealloc_full_page_cycle) {
    test_slab slab(32, 8);

    size_t page_offset = slab_page_offset(slab->object_align);
    size_t chunks_per_page = (SLAB_PAGE - page_offset) / slab->slot_size;

    std::vector<void*> ptrs;

    slab.on_after_alloc = [](test_slab& slab) {
        ASSERT_EQ(slab.pages.size(), 1);
    };

    // Fill a page
    ASSERT_EQ(slab.pages.size(), 0);
    for (size_t i = 0; i < chunks_per_page; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }
    ASSERT_EQ(slab.pages.size(), 1);

    // Deallocate all
    for (auto ptr : ptrs) {
        slab_dealloc(slab.get(), ptr);
    }
    ASSERT_EQ(slab.pages.size(), 0);

    // Fill a page
    for (size_t i = 0; i < chunks_per_page; i++) {
        ASSERT_TRUE(slab_alloc(slab.get()));
    }
    ASSERT_EQ(slab.pages.size(), 1);
}

TEST(slab_test, alloc_dealloc_interleaved_pages) {
    test_slab slab(64, 8);

    size_t page_offset = slab_page_offset(slab->object_align);
    size_t chunks_per_page = (SLAB_PAGE - page_offset) / slab->slot_size;

    std::vector<void*> ptrs;

    slab.on_after_alloc = [](test_slab& slab) {
        ASSERT_LE(slab.pages.size(), 2);
    };

    slab.on_before_dealloc = [](test_slab& slab) {
        FAIL();
    };

    // Allocate enough for two pages
    ASSERT_EQ(slab.pages.size(), 0);
    for (size_t i = 0; i < chunks_per_page * 2; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }
    ASSERT_EQ(slab.pages.size(), 2);

    slab.on_after_alloc = [](test_slab& slab) {
        FAIL();
    };

    // Deallocate every other chunk
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        slab_dealloc(slab.get(), ptrs[i]);
    }
    ASSERT_EQ(slab.pages.size(), 2);

    // Allocate again, should fill freed slots before allocating new pages
    for (size_t i = 0; i < chunks_per_page; i++) {
        ASSERT_TRUE(slab_alloc(slab.get()));
    }
    ASSERT_EQ(slab.pages.size(), 2);
}

TEST(slab_test, alloc_dealloc_many_types) {
    test_slab slab1(16, 8);
    test_slab slab2(32, 16);
    test_slab slab3(64, 32);
    void* ptr1 = slab_alloc(slab1.get());
    void* ptr2 = slab_alloc(slab2.get());
    void* ptr3 = slab_alloc(slab3.get());
    ASSERT_TRUE(ptr1);
    ASSERT_TRUE(ptr2);
    ASSERT_TRUE(ptr3);
    slab_dealloc(slab1.get(), ptr1);
    slab_dealloc(slab2.get(), ptr2);
    slab_dealloc(slab3.get(), ptr3);
}

TEST(slab_test, alloc_dealloc_zero_sized_type) {
    test_slab slab(0, 8);
    void* ptr1 = slab_alloc(slab.get());
    void* ptr2 = slab_alloc(slab.get());
    ASSERT_EQ((uintptr_t)ptr1 % 8, 0);
    ASSERT_EQ((uintptr_t)ptr2 % 8, 0);
    ASSERT_NE(ptr1, ptr2);
    slab_dealloc(slab.get(), ptr1);
    slab_dealloc(slab.get(), ptr2);
}

TEST(slab_test, alloc_dealloc_with_unusual_alignment) {
    test_slab slab(128, 256);
    void* ptr = slab_alloc(slab.get());
    ASSERT_TRUE(ptr);
    slab_dealloc(slab.get(), ptr);
}

TEST(slab_test, alloc_dealloc_with_minimum_size) {
    test_slab slab(1, 1);
    void* ptr = slab_alloc(slab.get());
    ASSERT_TRUE(ptr);
    slab_dealloc(slab.get(), ptr);
}

TEST(slab_test, alloc_dealloc_with_large_alignment) {
    test_slab slab(32, 1024);
    void* ptr = slab_alloc(slab.get());
    ASSERT_EQ((uintptr_t)ptr % 1024, 0);
    ASSERT_TRUE(ptr);
    slab_dealloc(slab.get(), ptr);
}

TEST(slab_test, alloc_dealloc_with_multiple_allocators) {
    std::vector<std::unique_ptr<test_slab>> slabs;
    for (int i = 0; i < 4; i++) {
        slabs.push_back(std::make_unique<test_slab>(32, 8));
    }

    std::vector<void*> ptrs;
    for (auto& slab : slabs) {
        void* ptr = slab_alloc(slab->get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }
    for (size_t i = 0; i < slabs.size(); i++) {
        slab_dealloc(slabs[i]->get(), ptrs[i]);
    }
}
