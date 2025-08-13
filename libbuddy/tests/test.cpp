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
#include <set>
#include <algorithm>

struct alignas(4096) page {
    uint64_t buf[512];
};

struct test_buddy {
    buddy_blocks buddy;
    std::vector<page> mem;
    test_buddy() {
        mem.resize(0x00200000 / 4096);
        buddy_init(&buddy, mem.data(), mem.size() * sizeof(page));
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
            if (const auto slice = (volatile uint32_t*)buddy_alloc(buddy.get(), size - 1)) {
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
            buddy_dealloc(buddy.get(), (void*)(addr + 1), size - 1);
        }

        ASSERT_EQ(buddy->used, 0);
    }
}

TEST(buddy_test, alloc_exact_unit_size) {
    test_buddy buddy;
    
    void* ptr = buddy_alloc(buddy.get(), BUDDY_UNIT);
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(buddy->used, BUDDY_UNIT);
    
    buddy_dealloc(buddy.get(), ptr, BUDDY_UNIT);
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, alloc_small_sizes) {
    test_buddy buddy;
    
    // 1 바이트부터 BUDDY_UNIT 미만까지 다양한 크기 할당
    std::vector<void*> ptrs;
    std::vector<size_t> sizes = {1, 16, 64, 256, 1024, 2048, BUDDY_UNIT - 1};
    
    for (size_t size : sizes) {
        void* ptr = buddy_alloc(buddy.get(), size);
        ASSERT_NE(ptr, nullptr) << "Failed to allocate " << size << " bytes";
        ptrs.push_back(ptr);
    }
    
    // 모든 할당이 BUDDY_UNIT 크기로 정렬됨
    ASSERT_EQ(buddy->used, sizes.size() * BUDDY_UNIT);
    
    // 모든 메모리 해제
    for (size_t i = 0; i < ptrs.size(); ++i) {
        buddy_dealloc(buddy.get(), ptrs[i], sizes[i]);
    }
    
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, alloc_zero_size) {
    test_buddy buddy;
    
    // 0 크기 할당은 assertion 실패해야 함
    EXPECT_DEATH(buddy_alloc(buddy.get(), 0), ".*");
}

TEST(buddy_test, dealloc_zero_size) {
    test_buddy buddy;
    
    void* ptr = buddy_alloc(buddy.get(), BUDDY_UNIT);
    ASSERT_NE(ptr, nullptr);
    
    // 0 크기 해제는 아무것도 하지 않음
    buddy_dealloc(buddy.get(), ptr, 0);
    ASSERT_EQ(buddy->used, BUDDY_UNIT);  // 여전히 할당된 상태
    
    buddy_dealloc(buddy.get(), ptr, BUDDY_UNIT);
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, large_allocation_failure) {
    test_buddy buddy;
    
    // 전체 메모리보다 큰 할당 요청
    size_t total_data_size = buddy->total_len - buddy->data_offset;
    void* ptr = buddy_alloc(buddy.get(), total_data_size + 1);
    ASSERT_EQ(ptr, nullptr);
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, maximum_level_allocation) {
    test_buddy buddy;
    
    // 가장 큰 블록 할당
    size_t max_level = buddy->levels - 1;
    size_t max_level_size = BUDDY_UNIT << max_level;
    
    slice s = buddy_alloc_slice(buddy.get(), max_level_size);
    ASSERT_NE(s.ptr, nullptr);
    ASSERT_EQ(s.length, max_level_size);
    ASSERT_EQ(buddy->used, max_level_size);
    
    // 이미 할당된 상태에서 추가 할당 시도
    slice s2 = buddy_alloc_slice(buddy.get(), max_level_size);
    ASSERT_EQ(s2.ptr, nullptr);  // 메모리 부족
    ASSERT_EQ(s2.length, 0);

    buddy_dealloc(buddy.get(), s.ptr, s.length);
    ASSERT_EQ(buddy->used, 0);
    
    // 해제 후 다시 할당 가능
    s2 = buddy_alloc_slice(buddy.get(), BUDDY_UNIT);
    ASSERT_NE(s2.ptr, nullptr);
    ASSERT_EQ(s2.length, BUDDY_UNIT);

    buddy_dealloc(buddy.get(), s2.ptr, s2.length);
}

TEST(buddy_test, fragmentation_and_coalescing) {
    test_buddy buddy;
    
    // 여러 개의 작은 블록 할당
    std::vector<void*> ptrs;
    for (int i = 0; i < 8; ++i) {
        void* ptr = buddy_alloc(buddy.get(), BUDDY_UNIT);
        ASSERT_NE(ptr, nullptr);
        ptrs.push_back(ptr);
    }
    
    ASSERT_EQ(buddy->used, 8 * BUDDY_UNIT);
    
    // 일부만 해제하여 단편화 생성
    buddy_dealloc(buddy.get(), ptrs[1], BUDDY_UNIT);
    buddy_dealloc(buddy.get(), ptrs[3], BUDDY_UNIT);
    buddy_dealloc(buddy.get(), ptrs[5], BUDDY_UNIT);
    buddy_dealloc(buddy.get(), ptrs[7], BUDDY_UNIT);
    
    ASSERT_EQ(buddy->used, 4 * BUDDY_UNIT);
    
    // 나머지도 해제 (buddy system이 병합해야 함)
    buddy_dealloc(buddy.get(), ptrs[0], BUDDY_UNIT);
    buddy_dealloc(buddy.get(), ptrs[2], BUDDY_UNIT);
    buddy_dealloc(buddy.get(), ptrs[4], BUDDY_UNIT);
    buddy_dealloc(buddy.get(), ptrs[6], BUDDY_UNIT);
    
    ASSERT_EQ(buddy->used, 0);
    
    // 큰 블록 할당이 가능해야 함 (병합이 제대로 되었다면)
    void* large_ptr = buddy_alloc(buddy.get(), 8 * BUDDY_UNIT);
    ASSERT_NE(large_ptr, nullptr);
    
    buddy_dealloc(buddy.get(), large_ptr, 8 * BUDDY_UNIT);
}

TEST(buddy_test, address_alignment) {
    test_buddy buddy;
    
    for (int i = 0; i < 10; ++i) {
        void* ptr = buddy_alloc(buddy.get(), BUDDY_UNIT);
        ASSERT_NE(ptr, nullptr);
        
        // 모든 할당된 주소는 BUDDY_UNIT으로 정렬되어야 함
        ASSERT_EQ((uintptr_t)ptr % BUDDY_UNIT, 0) 
            << "Allocation " << i << " not aligned: " << ptr;
        
        buddy_dealloc(buddy.get(), ptr, BUDDY_UNIT);
    }
}

TEST(buddy_test, unaligned_dealloc) {
    test_buddy buddy;
    
    void* ptr = buddy_alloc(buddy.get(), BUDDY_UNIT);
    ASSERT_NE(ptr, nullptr);
    
    // 정렬되지 않은 주소와 크기로 해제
    char* unaligned_ptr = (char*)ptr + 100;
    buddy_dealloc(buddy.get(), unaligned_ptr, 500);
    
    ASSERT_EQ(buddy->used, 0);  // 내부적으로 정렬되어 해제됨
}

TEST(buddy_test, stress_random_allocation) {
    test_buddy buddy;
    
    std::vector<std::pair<void*, size_t>> allocations;
    const int iterations = 1000;
    
    for (int i = 0; i < iterations; ++i) {
        if (allocations.empty() || (rand() % 3 != 0)) {
            // 할당
            size_t size = (rand() % 8 + 1) * BUDDY_UNIT;
            void* ptr = buddy_alloc(buddy.get(), size);
            if (ptr != nullptr) {
                allocations.push_back({ptr, size});
                
                // 할당된 메모리에 패턴 쓰기
                uint32_t* data = (uint32_t*)ptr;
                for (size_t j = 0; j < size / sizeof(uint32_t); ++j) {
                    data[j] = i * 1000 + j;
                }
            }
        } else {
            // 해제
            size_t idx = rand() % allocations.size();
            auto [ptr, size] = allocations[idx];
            
            // 패턴 검증
            uint32_t* data = (uint32_t*)ptr;
            for (size_t j = 0; j < size / sizeof(uint32_t); ++j) {
                // 패턴이 손상되지 않았는지 확인하기 위해서는
                // 할당 시의 iteration 번호를 저장해야 하지만, 
                // 여기서는 단순히 메모리가 읽기 가능한지만 확인
                volatile uint32_t val = data[j];
                (void)val;
            }
            
            buddy_dealloc(buddy.get(), ptr, size);
            allocations.erase(allocations.begin() + idx);
        }
    }
    
    // 모든 할당 해제
    for (auto [ptr, size] : allocations) {
        buddy_dealloc(buddy.get(), ptr, size);
    }
    
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, memory_exhaustion) {
    test_buddy buddy;
    
    std::vector<void*> ptrs;
    
    // 메모리를 모두 소진할 때까지 할당
    while (true) {
        void* ptr = buddy_alloc(buddy.get(), BUDDY_UNIT);
        if (ptr == nullptr) {
            break;
        }
        ptrs.push_back(ptr);
    }
    
    // 추가 할당이 실패해야 함
    void* failed_ptr = buddy_alloc(buddy.get(), BUDDY_UNIT);
    ASSERT_EQ(failed_ptr, nullptr);
    
    size_t expected_used = ptrs.size() * BUDDY_UNIT;
    ASSERT_EQ(buddy->used, expected_used);
    
    // 모든 메모리 해제
    for (void* ptr : ptrs) {
        buddy_dealloc(buddy.get(), ptr, BUDDY_UNIT);
    }
    
    ASSERT_EQ(buddy->used, 0);
    
    // 해제 후 다시 할당 가능
    void* ptr = buddy_alloc(buddy.get(), BUDDY_UNIT);
    ASSERT_NE(ptr, nullptr);
    buddy_dealloc(buddy.get(), ptr, BUDDY_UNIT);
}

TEST(buddy_test, different_sizes_allocation) {
    test_buddy buddy;
    
    // 다양한 크기의 블록을 동시에 할당
    std::vector<std::pair<void*, size_t>> allocations;
    
    for (size_t level = 0; level < buddy->levels && level < 5; ++level) {
        size_t size = BUDDY_UNIT << level;
        void* ptr = buddy_alloc(buddy.get(), size);
        ASSERT_NE(ptr, nullptr) << "Failed to allocate level " << level;
        allocations.push_back({ptr, size});
    }
    
    // 모든 할당이 성공했는지 확인
    size_t total_expected = 0;
    for (auto [ptr, size] : allocations) {
        total_expected += size;
    }
    ASSERT_EQ(buddy->used, total_expected);
    
    // 역순으로 해제
    for (auto it = allocations.rbegin(); it != allocations.rend(); ++it) {
        buddy_dealloc(buddy.get(), it->first, it->second);
    }
    
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, boundary_addresses) {
    test_buddy buddy;
    
    void* ptr = buddy_alloc(buddy.get(), BUDDY_UNIT);
    ASSERT_NE(ptr, nullptr);
    
    uintptr_t data_start = buddy->start_addr + buddy->data_offset;
    uintptr_t data_end = buddy->start_addr + buddy->total_len;
    
    // 할당된 주소가 유효한 범위 내에 있는지 확인
    ASSERT_GE((uintptr_t)ptr, data_start);
    ASSERT_LT((uintptr_t)ptr, data_end);
    
    buddy_dealloc(buddy.get(), ptr, BUDDY_UNIT);
}

TEST(buddy_test, multiple_buddy_instances) {
    // 서로 다른 크기의 buddy 시스템 여러 개 생성
    std::vector<page> mem1(64);  // 작은 버디
    std::vector<page> mem2(256); // 큰 버디
    
    buddy_blocks buddy1, buddy2;
    buddy_init(&buddy1, mem1.data(), mem1.size() * sizeof(page));
    buddy_init(&buddy2, mem2.data(), mem2.size() * sizeof(page));
    
    // 각각 독립적으로 작동하는지 확인
    void* ptr1 = buddy_alloc(&buddy1, BUDDY_UNIT);
    void* ptr2 = buddy_alloc(&buddy2, BUDDY_UNIT);
    
    ASSERT_NE(ptr1, nullptr);
    ASSERT_NE(ptr2, nullptr);
    ASSERT_NE(ptr1, ptr2);  // 서로 다른 주소
    
    ASSERT_EQ(buddy1.used, BUDDY_UNIT);
    ASSERT_EQ(buddy2.used, BUDDY_UNIT);
    
    buddy_dealloc(&buddy1, ptr1, BUDDY_UNIT);
    buddy_dealloc(&buddy2, ptr2, BUDDY_UNIT);
    
    ASSERT_EQ(buddy1.used, 0);
    ASSERT_EQ(buddy2.used, 0);
}

TEST(buddy_test, alloc_slice_exact_unit_size) {
    test_buddy buddy;
    
    struct slice s = buddy_alloc_slice(buddy.get(), BUDDY_UNIT);
    ASSERT_NE(s.ptr, nullptr);
    ASSERT_EQ(s.length, BUDDY_UNIT);
    ASSERT_EQ(buddy->used, BUDDY_UNIT);
    
    buddy_dealloc(buddy.get(), s.ptr, s.length);
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, alloc_slice_small_sizes) {
    test_buddy buddy;
    
    // BUDDY_UNIT보다 작은 크기들
    std::vector<size_t> sizes = {1, 16, 64, 256, 1024, 2048, BUDDY_UNIT - 1};
    std::vector<struct slice> slices;
    
    for (size_t size : sizes) {
        struct slice s = buddy_alloc_slice(buddy.get(), size);
        ASSERT_NE(s.ptr, nullptr) << "Failed to allocate " << size << " bytes";
        ASSERT_EQ(s.length, BUDDY_UNIT) << "Wrong slice length for " << size << " bytes";
        slices.push_back(s);
    }
    
    ASSERT_EQ(buddy->used, sizes.size() * BUDDY_UNIT);
    
    // 모든 슬라이스 해제
    for (const auto& s : slices) {
        buddy_dealloc(buddy.get(), s.ptr, s.length);
    }
    
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, alloc_slice_large_sizes) {
    test_buddy buddy;
    
    // 다양한 레벨의 블록 크기들
    std::vector<struct slice> slices;
    
    for (size_t level = 0; level < buddy->levels && level < 6; ++level) {
        size_t size = BUDDY_UNIT << level;
        
        // 정확한 크기
        struct slice s1 = buddy_alloc_slice(buddy.get(), size);
        ASSERT_NE(s1.ptr, nullptr) << "Failed to allocate exact level " << level;
        ASSERT_EQ(s1.length, size);
        slices.push_back(s1);
        
        // 약간 작은 크기 (같은 레벨이어야 함)
        if (size > 1) {
            struct slice s2 = buddy_alloc_slice(buddy.get(), size - 1);
            ASSERT_NE(s2.ptr, nullptr) << "Failed to allocate level " << level << " - 1";
            ASSERT_EQ(s2.length, size);
            slices.push_back(s2);
        }
    }
    
    // 모든 슬라이스 해제
    for (const auto& s : slices) {
        buddy_dealloc(buddy.get(), s.ptr, s.length);
    }
    
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, alloc_slice_zero_size) {
    test_buddy buddy;
    
    // 0 크기 할당은 assertion 실패해야 함
    EXPECT_DEATH(buddy_alloc_slice(buddy.get(), 0), ".*");
}

TEST(buddy_test, alloc_slice_too_large) {
    test_buddy buddy;
    
    // 전체 메모리보다 큰 할당 요청
    size_t total_data_size = buddy->total_len - buddy->data_offset;
    struct slice s = buddy_alloc_slice(buddy.get(), total_data_size + 1);
    
    ASSERT_EQ(s.ptr, nullptr);
    ASSERT_EQ(s.length, 0);
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, alloc_slice_address_alignment) {
    test_buddy buddy;
    
    for (int i = 0; i < 10; ++i) {
        struct slice s = buddy_alloc_slice(buddy.get(), BUDDY_UNIT);
        ASSERT_NE(s.ptr, nullptr);
        ASSERT_EQ(s.length, BUDDY_UNIT);
        
        // 모든 할당된 주소는 BUDDY_UNIT으로 정렬되어야 함
        ASSERT_EQ((uintptr_t)s.ptr % BUDDY_UNIT, 0) 
            << "Slice " << i << " not aligned: " << s.ptr;
        
        buddy_dealloc(buddy.get(), s.ptr, s.length);
    }
}

TEST(buddy_test, alloc_slice_fragmentation_test) {
    test_buddy buddy;
    
    // 여러 개의 작은 블록 할당
    std::vector<struct slice> slices;
    for (int i = 0; i < 8; ++i) {
        struct slice s = buddy_alloc_slice(buddy.get(), BUDDY_UNIT);
        ASSERT_NE(s.ptr, nullptr);
        ASSERT_EQ(s.length, BUDDY_UNIT);
        slices.push_back(s);
    }
    
    ASSERT_EQ(buddy->used, 8 * BUDDY_UNIT);
    
    // 일부만 해제하여 단편화 생성
    buddy_dealloc(buddy.get(), slices[1].ptr, slices[1].length);
    buddy_dealloc(buddy.get(), slices[3].ptr, slices[3].length);
    buddy_dealloc(buddy.get(), slices[5].ptr, slices[5].length);
    buddy_dealloc(buddy.get(), slices[7].ptr, slices[7].length);
    
    ASSERT_EQ(buddy->used, 4 * BUDDY_UNIT);
    
    // 나머지도 해제
    buddy_dealloc(buddy.get(), slices[0].ptr, slices[0].length);
    buddy_dealloc(buddy.get(), slices[2].ptr, slices[2].length);
    buddy_dealloc(buddy.get(), slices[4].ptr, slices[4].length);
    buddy_dealloc(buddy.get(), slices[6].ptr, slices[6].length);
    
    ASSERT_EQ(buddy->used, 0);
    
    // 큰 블록 할당이 가능해야 함 (병합이 제대로 되었다면)
    struct slice large_s = buddy_alloc_slice(buddy.get(), 8 * BUDDY_UNIT);
    ASSERT_NE(large_s.ptr, nullptr);
    ASSERT_EQ(large_s.length, 8 * BUDDY_UNIT);
    
    buddy_dealloc(buddy.get(), large_s.ptr, large_s.length);
}

TEST(buddy_test, alloc_slice_memory_exhaustion) {
    test_buddy buddy;
    
    std::vector<struct slice> slices;
    
    // 메모리를 모두 소진할 때까지 할당
    while (true) {
        struct slice s = buddy_alloc_slice(buddy.get(), BUDDY_UNIT);
        if (s.ptr == nullptr) {
            ASSERT_EQ(s.length, 0);
            break;
        }
        ASSERT_EQ(s.length, BUDDY_UNIT);
        slices.push_back(s);
    }
    
    // 추가 할당이 실패해야 함
    struct slice failed_s = buddy_alloc_slice(buddy.get(), BUDDY_UNIT);
    ASSERT_EQ(failed_s.ptr, nullptr);
    ASSERT_EQ(failed_s.length, 0);
    
    size_t expected_used = slices.size() * BUDDY_UNIT;
    ASSERT_EQ(buddy->used, expected_used);
    
    // 모든 메모리 해제
    for (const auto& s : slices) {
        buddy_dealloc(buddy.get(), s.ptr, s.length);
    }
    
    ASSERT_EQ(buddy->used, 0);
    
    // 해제 후 다시 할당 가능
    struct slice s = buddy_alloc_slice(buddy.get(), BUDDY_UNIT);
    ASSERT_NE(s.ptr, nullptr);
    ASSERT_EQ(s.length, BUDDY_UNIT);
    buddy_dealloc(buddy.get(), s.ptr, s.length);
}

TEST(buddy_test, alloc_slice_mixed_sizes) {
    test_buddy buddy;
    
    std::vector<struct slice> slices;
    
    // 다양한 크기의 블록을 동시에 할당
    for (size_t level = 0; level < buddy->levels && level < 5; ++level) {
        size_t size = BUDDY_UNIT << level;
        struct slice s = buddy_alloc_slice(buddy.get(), size);
        ASSERT_NE(s.ptr, nullptr) << "Failed to allocate level " << level;
        ASSERT_EQ(s.length, size);
        slices.push_back(s);
    }
    
    // 모든 할당이 성공했는지 확인
    size_t total_expected = 0;
    for (const auto& s : slices) {
        total_expected += s.length;
    }
    ASSERT_EQ(buddy->used, total_expected);
    
    // 역순으로 해제
    for (auto it = slices.rbegin(); it != slices.rend(); ++it) {
        buddy_dealloc(buddy.get(), it->ptr, it->length);
    }
    
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, alloc_slice_boundary_addresses) {
    test_buddy buddy;
    
    struct slice s = buddy_alloc_slice(buddy.get(), BUDDY_UNIT);
    ASSERT_NE(s.ptr, nullptr);
    ASSERT_EQ(s.length, BUDDY_UNIT);
    
    uintptr_t data_start = buddy->start_addr + buddy->data_offset;
    uintptr_t data_end = buddy->start_addr + buddy->total_len;
    
    // 할당된 주소가 유효한 범위 내에 있는지 확인
    ASSERT_GE((uintptr_t)s.ptr, data_start);
    ASSERT_LT((uintptr_t)s.ptr, data_end);
    
    // 슬라이스가 범위를 벗어나지 않는지 확인
    ASSERT_LE((uintptr_t)s.ptr + s.length, data_end);
    
    buddy_dealloc(buddy.get(), s.ptr, s.length);
}

TEST(buddy_test, alloc_slice_stress_random) {
    test_buddy buddy;
    
    std::vector<struct slice> allocations;
    const int iterations = 500;
    
    for (int i = 0; i < iterations; ++i) {
        if (allocations.empty() || (rand() % 3 != 0)) {
            // 할당
            size_t level = rand() % std::min(4U, buddy->levels);
            size_t size = BUDDY_UNIT << level;
            struct slice s = buddy_alloc_slice(buddy.get(), size);
            if (s.ptr != nullptr) {
                ASSERT_EQ(s.length, size);
                allocations.push_back(s);
                
                // 할당된 메모리에 패턴 쓰기
                uint32_t* data = (uint32_t*)s.ptr;
                for (size_t j = 0; j < s.length / sizeof(uint32_t); ++j) {
                    data[j] = i * 1000 + j;
                }
            }
        } else {
            // 해제
            size_t idx = rand() % allocations.size();
            struct slice s = allocations[idx];
            
            // 패턴 검증
            uint32_t* data = (uint32_t*)s.ptr;
            for (size_t j = 0; j < s.length / sizeof(uint32_t); ++j) {
                volatile uint32_t val = data[j];
                (void)val; // 메모리가 읽기 가능한지만 확인
            }
            
            buddy_dealloc(buddy.get(), s.ptr, s.length);
            allocations.erase(allocations.begin() + idx);
        }
    }
    
    // 모든 할당 해제
    for (const auto& s : allocations) {
        buddy_dealloc(buddy.get(), s.ptr, s.length);
    }
    
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, alloc_slice_unaligned_sizes) {
    test_buddy buddy;
    
    // 정렬되지 않은 크기들 - 모두 BUDDY_UNIT으로 올림되어야 함
    std::vector<size_t> unaligned_sizes = {
        BUDDY_UNIT + 1, BUDDY_UNIT + 100, BUDDY_UNIT + BUDDY_UNIT/2,
        2*BUDDY_UNIT + 1, 3*BUDDY_UNIT - 1, 4*BUDDY_UNIT + 50
    };
    
    std::vector<size_t> expected_sizes = {
        2*BUDDY_UNIT, 2*BUDDY_UNIT, 2*BUDDY_UNIT,
        4*BUDDY_UNIT, 4*BUDDY_UNIT, 8*BUDDY_UNIT
    };
    
    std::vector<struct slice> slices;
    
    for (size_t i = 0; i < unaligned_sizes.size(); ++i) {
        struct slice s = buddy_alloc_slice(buddy.get(), unaligned_sizes[i]);
        ASSERT_NE(s.ptr, nullptr) << "Failed to allocate " << unaligned_sizes[i];
        ASSERT_EQ(s.length, expected_sizes[i]) << "Wrong size for " << unaligned_sizes[i];
        slices.push_back(s);
    }
    
    // 모든 슬라이스 해제
    for (const auto& s : slices) {
        buddy_dealloc(buddy.get(), s.ptr, s.length);
    }
    
    ASSERT_EQ(buddy->used, 0);
}

TEST(buddy_test, alloc_slice_exact_power_of_two) {
    test_buddy buddy;
    
    // 2의 거듭제곱 크기들
    std::vector<struct slice> slices;
    
    for (size_t level = 0; level < buddy->levels && level < 6; ++level) {
        size_t size = BUDDY_UNIT << level;
        struct slice s = buddy_alloc_slice(buddy.get(), size);
        ASSERT_NE(s.ptr, nullptr) << "Failed to allocate 2^" << level << " * BUDDY_UNIT";
        ASSERT_EQ(s.length, size) << "Wrong slice length for level " << level;
        
        // offset이 해당 크기로 정렬되어야 함
        uintptr_t offset = (uintptr_t)s.ptr - (buddy->start_addr + buddy->data_offset);
        ASSERT_EQ(offset % size, 0) << "Level " << level << " not properly aligned";

        slices.push_back(s);
    }
    
    // 모든 슬라이스 해제
    for (const auto& s : slices) {
        buddy_dealloc(buddy.get(), s.ptr, s.length);
    }
    
    ASSERT_EQ(buddy->used, 0);
}

