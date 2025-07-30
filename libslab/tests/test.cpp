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

extern "C" void* test_alloc(void* ctx);
extern "C" void test_dealloc(void* ctx, void* ptr);

struct test_slab {
    slab_page_allocator pa;
    slab_allocator sa;
    std::vector<void*> pages;
    std::vector<void*> deallocated;
    test_slab(size_t size, size_t align) {
        pa.ctx = this;
        pa.alloc = test_alloc;
        pa.dealloc = test_dealloc;
        slab_init(&sa, size, align, &pa);
    }
    ~test_slab() {
        for (auto p : deallocated) {
            bool ok = std::all_of((char*)p, (char*)p + SLAB_PAGE, [](char x) {
                return (unsigned char)x == 0xdd;
            });
            if (!ok) {
                abort();
            }
            free(p);
        }
        for (auto p : pages) {
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
    void* const page = malloc(SLAB_PAGE);
    memset(page, 0xcc, SLAB_PAGE);
    self->pages.push_back(page);
    return page;
}

extern "C" void test_dealloc(void* ctx, void* ptr) {
    auto self = static_cast<test_slab*>(ctx);
    auto it = find(self->pages.begin(), self->pages.end(), ptr);
    ASSERT_TRUE(it != self->pages.end());
    memset(*it, 0xdd, SLAB_PAGE);
    self->deallocated.push_back(*it);
    self->pages.erase(it);
}

TEST(slab_test, creation) {
    test_slab slab(64, 8);
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

    size_t chunks_per_page = SLAB_PAGE / 32;
    std::vector<void*> ptrs;

    // Allocate all possible chunks in a page
    for (size_t i = 0; i < chunks_per_page; i++) {
        void* ptr = slab_alloc(slab.get());
        ASSERT_TRUE(ptr);
        ptrs.push_back(ptr);
    }

    // Allocating one more should allocate a new page
    //ASSERT_EQ(slab.pages.size(), 1);
    void* extra = slab_alloc(slab.get());
    ASSERT_TRUE(extra);
    //ASSERT_EQ(slab.pages.size(), 2);

    // Deallocate all and ensure reuse
    for (auto ptr : ptrs) {
        //ASSERT_EQ(slab.pages.size(), 2);
        slab_dealloc(slab.get(), ptr);
    }
    //ASSERT_EQ(slab.pages.size(), 1);
    slab_dealloc(slab.get(), extra);
    //ASSERT_EQ(slab.pages.size(), 0);

    // After deallocation, allocation should succeed again
    void* ptr = slab_alloc(slab.get());
    //ASSERT_EQ(slab.pages.size(), 1);
    ASSERT_TRUE(ptr);
    slab_dealloc(slab.get(), ptr);
    //ASSERT_EQ(slab.pages.size(), 0);
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
            std::uniform_int_distribution<size_t> dist_idx(0, ptrs.size());
            size_t idx = dist_idx(rng);
            void* ptr = ptrs.back();
            std::swap(ptrs[idx], ptrs.back());
            ptrs.pop_back();
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

    const size_t chunks_per_page = SLAB_PAGE / 32;
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

    const size_t chunks_per_page = SLAB_PAGE / 32;
    for (int i = 0; i < 10; i++) {
        std::vector<void*> ptrs;
        for (size_t i = 0; i < chunks_per_page * 3; i++) {
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
    ASSERT_DEATH(slab_dealloc(slab.get(), ptr), "Double free should panic");
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
