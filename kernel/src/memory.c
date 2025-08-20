#include <stdalign.h>
#include <freec/stdlib.h>
#include <freec/string.h>
#include <freec/assert.h>
#include <buddy/buddy.h>
#include <slab/slab.h>
#include <collections/singlylist.h>

#include "memory.h"
#include "boot.h"
#include "spinlock.h"
#include "tty.h"

#if PAGE_SIZE != BUDDY_UNIT || PAGE_SIZE != SLAB_PAGE
#error PAGE_SIZE must be equal to BUDDY_UNIT and SLAB_PAGE
#endif

union mmap_buffer {
    struct {
        uint32_t len;
        uint32_t reserved;
        struct mmap_entry entries[BOOTINFO_MMAP_MAXLEN];
    };
    struct mmap mmap;
};

struct mmio_free_node {
    struct singlylist_link link;
    uintptr_t begin; // zero if invalid
    uintptr_t end;
};

struct mmio_freelist {
    struct singlylist list;
    struct mmio_free_node nodes[24];
};

struct meminfo {
    struct intrlock lock;
    struct mmio_freelist mmio_freelist;
    struct buddy_blocks buddy;
    size_t dyn_total_len;
    size_t dyn_pagetable_len;
};

static union mmap_buffer g_mmap_dyn;
static struct meminfo g_meminfo;

static void* slab_page_alloc(void* ctx) {
    return dynmem_alloc(SLAB_PAGE).ptr;
}

static void slab_page_dealloc(void* ctx, void* page) {
    dynmem_dealloc(page, SLAB_PAGE);
}

struct slab_page_allocator g_slab_page_allocator = {
    .alloc = slab_page_alloc,
    .dealloc = slab_page_dealloc,
    .ctx = NULL,
};

static void* arraylist_alloc(size_t len, size_t* allocated_len) {
    size_t aligned = szdiv_ceil(len, PAGE_SIZE) * PAGE_SIZE;
    struct slice s = dynmem_alloc(aligned);
    *allocated_len = s.length;
    return s.ptr;
}

static void arraylist_dealloc(void* ptr, size_t len) {
    dynmem_dealloc(ptr, len);
}

static size_t arraylist_shrink(void* ptr, size_t old_len, size_t new_len) {
    size_t aligned_old = szdiv_ceil(old_len, PAGE_SIZE) * PAGE_SIZE;
    size_t aligned_new = szdiv_ceil(new_len, PAGE_SIZE) * PAGE_SIZE;
    if (aligned_old == aligned_new) {
        return aligned_old;
    }
    dynmem_dealloc((char*)ptr + aligned_new, aligned_old - aligned_new);
    return aligned_new;
}

struct arraylist_allocator g_arraylist_allocator = {
    .alloc = arraylist_alloc,
    .dealloc = arraylist_dealloc,
    .shrink = arraylist_shrink,
};

static int comp_mmap_entry(const void* a, const void* b) {
    const struct mmap_entry *ae = a, *be = b;
    return ae->base > be->base ? 1 : (ae->base < be->base ? -1 : 0);
}

static void construct_mmap_dyn(const struct mmap* mmap_boot) {
    g_mmap_dyn.len = mmap_boot->len;
    memcpy(&g_mmap_dyn.entries, &mmap_boot->entries, mmap_boot->len * sizeof(struct mmap_entry));
    sort(g_mmap_dyn.entries, g_mmap_dyn.len, sizeof(struct mmap_entry), comp_mmap_entry);

    size_t prev_end = DYNMEM_START_PHYS_MINIMUM;
    for (size_t i = 0; i < g_mmap_dyn.len; i++) {
        struct mmap_entry* entry = g_mmap_dyn.entries + i;
        if (entry->type != MMAP_ENTRY_AVAILABLE) {
            continue;
        }

        size_t start_aligned = szdiv_ceil(entry->base, PAGE_SIZE) * PAGE_SIZE;
        size_t end_aligned = (entry->base + entry->length) / PAGE_SIZE * PAGE_SIZE;
        if (start_aligned < prev_end) {
            start_aligned = prev_end;
        }
        if (start_aligned >= end_aligned) {
            entry->type = MMAP_ENTRY_RESERVED;
            continue;
        }

        entry->base = start_aligned;
        entry->length = end_aligned - start_aligned;
        prev_end = end_aligned;
    }

    size_t removal_end = 0;
    for (size_t i = 0; i < g_mmap_dyn.len; i++) {
        struct mmap_entry* entry = g_mmap_dyn.entries + i;
        if (entry->type == MMAP_ENTRY_AVAILABLE) {
            g_mmap_dyn.entries[removal_end++] = *entry;
        }
    }
    g_mmap_dyn.len = removal_end;
}

static void mmio_freelist_init(void) {
    struct mmio_freelist* freelist = &g_meminfo.mmio_freelist;
    singlylist_init(&freelist->list);
    memset(freelist->nodes, 0, sizeof(freelist->nodes));
    freelist->nodes[0].begin = IOMAP_START_VIRT;
    freelist->nodes[0].end = IOMAP_START_VIRT + IOMAP_VIRT_SIZE;
    singlylist_push_front(&freelist->list, &freelist->nodes[0].link);
}

static struct mmio_free_node* mmio_freelist_newnode(struct mmio_freelist* mfl) {
    size_t sz = sizeof(mfl->nodes) / sizeof(*mfl->nodes);
    size_t i = 0;
    for (; i < sz && mfl->nodes[i].begin != 0; i++) {}
    if (i == sz) {
        panic("mmio virtual memory space is too fragmented");
    }
    return mfl->nodes + i;
}

// TODO: unit tests for mmio_*
volatile void* mmio_alloc_mapping(uintptr_t begin_phys, uintptr_t end_phys) {
    intrlock_acquire(&g_meminfo.lock);

    uintptr_t aligned_begin_phys = begin_phys / PAGE_SIZE * PAGE_SIZE;
    uintptr_t aligned_len = uptrdiv_ceil(end_phys - aligned_begin_phys, PAGE_SIZE) * PAGE_SIZE;

    struct mmio_freelist* freelist = &g_meminfo.mmio_freelist;
    struct singlylist_link* before = singlylist_before_head(&freelist->list);
    struct singlylist_link* link = before->next;
    uintptr_t begin;
    for (; link != NULL; before = link, link = link->next) {
        struct mmio_free_node* node = container_of(link, struct mmio_free_node, link);

        begin = node->begin;
        uintptr_t node_len = node->end - begin;
        if (node_len < aligned_len) {
            continue;
        }

        if (node_len == aligned_len) {
            node->begin = 0;
            singlylist_remove_after(before);
        } else {
            node->begin += aligned_len;
        }
        break;
    }
    assert(link != NULL, "mmio virtual memory space is run out");

    pagetable_mmio_map(begin, begin + aligned_len, aligned_begin_phys, KERNEL_PAGE_FLAG, &g_mmap_dyn.mmap);

    intrlock_release(&g_meminfo.lock);
    return (void*)begin;
}

void mmio_dealloc_mapping(uintptr_t begin_virt, uintptr_t end_virt) {
    intrlock_acquire(&g_meminfo.lock);

    uintptr_t aligned_begin = begin_virt / PAGE_SIZE * PAGE_SIZE;
    uintptr_t aligned_len = uptrdiv_ceil(end_virt - aligned_begin, PAGE_SIZE) * PAGE_SIZE;
    uintptr_t aligned_end = aligned_begin + aligned_len;

    struct mmio_freelist* freelist = &g_meminfo.mmio_freelist;
    struct singlylist_link* before_head = singlylist_before_head(&freelist->list);
    struct singlylist_link* before = before_head;
    struct singlylist_link* link = before->next;
    for (; link != NULL; before = link, link = link->next) {
        struct mmio_free_node* before_node = container_of(before, struct mmio_free_node, link);
        struct mmio_free_node* node = container_of(link, struct mmio_free_node, link);

        if (aligned_begin >= node->begin) {
            continue;
        }

        uintptr_t before_end = before != before_head ? before_node->end : IOMAP_START_VIRT;
        assert(before_end <= aligned_begin && aligned_end <= node->begin, "invalid mmio address");

        if (aligned_begin + aligned_len == node->begin) {
            node->begin = aligned_begin;
        } else if (before != before_head && before_node->end == aligned_begin) {
            before_node->end += aligned_len;
        } else {
            struct mmio_free_node* newnode = mmio_freelist_newnode(freelist);
            newnode->begin = aligned_begin;
            newnode->end = aligned_begin + aligned_len;
            singlylist_insert_after(before, &newnode->link);
            break;
        }

        if (before != before_head && before_node->end == node->begin) {
            before_node->end = node->end;
            node->begin = 0;
            singlylist_remove_after(before);
        }
        break;
    }
    assert(link != NULL, "invalid mmio address");

    pagetable_mmio_unmap(aligned_begin, aligned_begin + aligned_len, &g_mmap_dyn.mmap);

    intrlock_release(&g_meminfo.lock);
}

struct slice dynmem_alloc_nolock(size_t len) {
    return buddy_alloc_slice(&g_meminfo.buddy, len);
}

struct slice dynmem_alloc(size_t len) {
    intrlock_acquire(&g_meminfo.lock);
    struct slice s = dynmem_alloc_nolock(len);
    intrlock_release(&g_meminfo.lock);
    return s;
}

void dynmem_dealloc_nolock(void* ptr, size_t len) {
    buddy_dealloc(&g_meminfo.buddy, ptr, len);
}

void dynmem_dealloc(void* ptr, size_t len) {
    intrlock_acquire(&g_meminfo.lock);
    dynmem_dealloc_nolock(ptr, len);
    intrlock_release(&g_meminfo.lock);
}

void memory_init(void) {
    intrlock_init(&g_meminfo.lock);

    construct_mmap_dyn(bootinfo_get()->mmap);

    struct pagetable_construct_result r = pagetable_construct(&g_mmap_dyn.mmap);
    g_meminfo.dyn_total_len = r.dyn_total_len;
    g_meminfo.dyn_pagetable_len = r.dyn_pagetable_len;

    buddy_init(&g_meminfo.buddy,
        (void*)(DYNMEM_START_VIRT + r.dyn_pagetable_len),
        r.dyn_total_len - r.dyn_pagetable_len);

    mmio_freelist_init();
}

static void mmap_print(const struct mmap* mmap, const char* title) {
    intrlock_acquire(&g_meminfo.lock);

    tty0_printf("%s: %d entries\n", title, mmap->len);
    for (uint32_t i = 0; i < mmap->len; i++) {
        const struct mmap_entry* entry = &mmap->entries[i];
        const char* type_str = mmap_entry_type_str(entry->type);
        if (type_str) {
            tty0_printf("    [0x%016"PRImul", 0x%016"PRImul") %s\n",
                entry->base, entry->base + entry->length, mmap_entry_type_str(entry->type));
        } else {
            tty0_printf("    [0x%016"PRImul", 0x%016"PRImul") (unknown:%"PRImet")\n",
                entry->base, entry->base + entry->length, entry->type);
        }
    }

    intrlock_release(&g_meminfo.lock);
}

void mmap_print_bootinfo(void) {
    mmap_print(bootinfo_get()->mmap, "System Memory Map");
}

void mmap_print_dyn(void) {
    mmap_print(&g_mmap_dyn.mmap, "Dynamic Memory Map");
}

void pagetable_print(void) {
    intrlock_acquire(&g_meminfo.lock);
    pagetable_print_with_dyn(&g_mmap_dyn.mmap);
    intrlock_release(&g_meminfo.lock);
}

void dynmem_print(void) {
    intrlock_acquire(&g_meminfo.lock);

    tty0_printf("===dynamic memory allocator infomation===\n");
    tty0_printf("metadata address     : %#018zx\n", g_meminfo.buddy.start_addr);
    tty0_printf("metadata size        : %#018zx\n", g_meminfo.buddy.metadata_len);
    tty0_printf("count of unit blocks : %#018zx\n", g_meminfo.buddy.units);
    tty0_printf("total bitmap level   : %u\n", g_meminfo.buddy.levels);
    tty0_printf("=========================================\n");
    tty0_printf("start address        : %#018zx\n", g_meminfo.buddy.start_addr + g_meminfo.buddy.data_offset);
    tty0_printf("dynmem size          : %#018zx\n", g_meminfo.buddy.total_len - g_meminfo.buddy.data_offset);
    tty0_printf("used size            : %#018zx\n", g_meminfo.buddy.used);
    tty0_printf("=========================================\n");

    intrlock_release(&g_meminfo.lock);
}

void dynmem_test_seq(void) {
    intrlock_acquire(&g_meminfo.lock);

    struct buddy_blocks* buddy = &g_meminfo.buddy;

    uintptr_t data_addr = buddy->start_addr + buddy->data_offset;
    tty0_printf("memory chunk starts at %#zx\n", data_addr);
    tty0_printf("data range: [%#zx, %#zx)\n", data_addr, buddy->start_addr + buddy->total_len);

    for (size_t level = 0; level < buddy->levels; level++) {
        const size_t block_count = buddy->units >> level;
        const size_t size = BUDDY_UNIT << level;

        tty0_printf("Bitmap Level #%zu (block_count=%zu, size=%#zx)\n", level, block_count, size);
        assert(buddy->used == 0);

        tty0_printf("Alloc & Comp : ");
        for (size_t index = 0; index < block_count; index++) {
            volatile uint32_t* slice = dynmem_alloc(size - 1).ptr;
            if (slice) {
                const size_t count = size / 4;
                for (size_t i = 0; i < count; i++) {
                    slice[i] = (uint32_t)i;
                }
                for (size_t i = 0; i < count; i++) {
                    assertf(slice[i] == (uint32_t)i, "comparison fail: level=%zu size=%zu index=%zu\n", level, size, index);
                }
                tty0_printf(".");
            } else {
                assertf(false, "alloc() fail: level=%zu size=%zu index=%zu\n", level, size, index);
            }
        }

        assert(buddy->used == (buddy->total_len - buddy->data_offset) / size * size);

        tty0_printf("\nDeallocation : ");
        for (size_t index = 0; index < block_count; index++) {
            const uintptr_t addr = buddy->start_addr + buddy->data_offset + size * index;
            dynmem_dealloc((void*)(addr + 1), size - 1);
            tty0_printf(".");
        }

        assert(buddy->used == 0);
        tty0_printf("\n");
    }
    
    tty0_printf("Done.\n");

    intrlock_release(&g_meminfo.lock);
}
