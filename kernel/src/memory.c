#include <stdalign.h>
#include <freec/stdlib.h>
#include <freec/string.h>
#include <freec/assert.h>
#include <buddy/buddy.h>
#include <slab/slab.h>
#include <collections/singlylist.h>

#include "arch/memory.h"
#include "memory.h"
#include "boot.h"

#include "drivers/serial.h"

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

static union mmap_buffer g_mmap_dyn;

struct mmio_free_node {
    struct singlylist_link link;
    uintptr_t begin; // zero if invalid
    uintptr_t end;
};

struct mmio_freelist {
    struct singlylist list;
    struct mmio_free_node nodes[24];
};

static struct mmio_freelist g_mmio_freelist;

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
    singlylist_init(&g_mmio_freelist.list);
    memset(g_mmio_freelist.nodes, 0, sizeof(g_mmio_freelist.nodes));
    g_mmio_freelist.nodes[0].begin = IOMAP_START_VIRT;
    g_mmio_freelist.nodes[0].end = IOMAP_START_VIRT + IOMAP_VIRT_SIZE;
    singlylist_push_front(&g_mmio_freelist.list, &g_mmio_freelist.nodes[0].link);
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

uintptr_t mmio_alloc_mapping(uintptr_t begin_phys, uintptr_t end_phys) {
    uintptr_t aligned_begin_phys = begin_phys / PAGE_SIZE * PAGE_SIZE;
    uintptr_t aligned_len = uptrdiv_ceil(end_phys - aligned_begin_phys, PAGE_SIZE) * PAGE_SIZE;

    struct singlylist_link* before = singlylist_before_head(&g_mmio_freelist.list);
    struct singlylist_link* link = before->next;
    uintptr_t begin;
    for (; link != NULL; before = link, link = link->next) {
        struct mmio_free_node* node = objectof(link, struct mmio_free_node, link);

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

    pagetable_map(begin, begin + aligned_len, aligned_begin_phys, KERNEL_PAGE_FLAG);
    return begin;
}

void mmio_dealloc_mapping(uintptr_t begin_virt, uintptr_t end_virt) {
    uintptr_t aligned_begin = begin_virt / PAGE_SIZE * PAGE_SIZE;
    uintptr_t aligned_len = uptrdiv_ceil(end_virt - aligned_begin, PAGE_SIZE) * PAGE_SIZE;
    uintptr_t aligned_end = aligned_begin + aligned_len;

    struct singlylist_link* before_head = singlylist_before_head(&g_mmio_freelist.list);
    struct singlylist_link* before = before_head;
    struct singlylist_link* link = before->next;
    for (; link != NULL; before = link, link = link->next) {
        struct mmio_free_node* before_node = objectof(before, struct mmio_free_node, link);
        struct mmio_free_node* node = objectof(link, struct mmio_free_node, link);

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
            struct mmio_free_node* newnode = mmio_freelist_newnode(&g_mmio_freelist);
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

    pagetable_map(aligned_begin, aligned_begin + aligned_len, 0, PAGE_FLAG_NIL);
}

void mmap_init(void) {
    construct_mmap_dyn(bootinfo_get()->mmap);
    pagetable_construct(&g_mmap_dyn.mmap);
    if (0) mmio_freelist_init();
}

static void mmap_print(const struct mmap* mmap, const char* title) {
    serial_printf("%s: %d entries\n", title, mmap->len);
    for (uint32_t i = 0; i < mmap->len; i++) {
        const struct mmap_entry* entry = &mmap->entries[i];
        const char* type_str = mmap_entry_type_str(entry->type);
        if (type_str) {
            serial_printf("    [0x%016"PRImul", 0x%016"PRImul") %s\n",
                entry->base, entry->base + entry->length, mmap_entry_type_str(entry->type));
        } else {
            serial_printf("    [0x%016"PRImul", 0x%016"PRImul") (unknown:%"PRImet")\n",
                entry->base, entry->base + entry->length, entry->type);
        }
    }
}

void mmap_print_bootinfo(void) {
    mmap_print(bootinfo_get()->mmap, "System Memory Map");
}

void mmap_print_dyn(void) {
    mmap_print(&g_mmap_dyn.mmap, "Dynamic Memory Map");
}

void pagetable_print(void) {
    pagetable_print_with_dyn(&g_mmap_dyn.mmap);
}
