#include "slab/slab.h"
#include <stdalign.h>
#include <stdbool.h>
#include <freec/stdlib.h>
#include <freec/string.h>
#include <freec/assert.h>

#define REDZONE_SIZE 16

#define EMPTY_MAGIC 0x3a49
#define OBJECT_MAGIC 0x6b5c
#define REDZONE_FILL 0xf1
#define UNUSED_FILL 0xf2

struct page {
    struct linkedlist_link link;
    uint16_t free_index;
    uint16_t alloc_count;
};

struct slot {
    uint16_t magic;
    uint16_t next;
};

struct object_info {
    size_t size;
    size_t align;
};

static size_t align_ceil(size_t x, size_t align) {
    const size_t mask = align - 1;
    return (x + mask) & ~mask;
}

static size_t slot_alignof(size_t object_align) {
    const size_t header_align = alignof(struct slot);
    return object_align > header_align ? object_align : header_align;
}

static size_t slot_redzone1_offset() {
    return sizeof(struct slot);
}

static size_t slot_payload_offset(size_t object_align) {
    return align_ceil(slot_redzone1_offset() + REDZONE_SIZE, object_align);
}

static size_t slot_redzone1_size(size_t object_align) {
    return slot_payload_offset(object_align) - slot_redzone1_offset();
}

static size_t slot_redzone2_offset(struct object_info info) {
    return slot_payload_offset(info.align) + info.size;
}

static size_t slot_sizeof(struct object_info info) {
    return align_ceil(slot_redzone2_offset(info) + REDZONE_SIZE, info.align);
}

static size_t slot_redzone2_size(struct object_info info) {
    return slot_sizeof(info) - slot_redzone2_offset(info);
}

static char* slot_redzone1(struct slot* slot) {
    return (char*)slot + slot_redzone1_offset();
}

static void* slot_payload(struct slot* slot, size_t object_align) {
    return (char*)slot + slot_payload_offset(object_align);
}

static char* slot_redzone2(struct slot* slot, struct object_info info) {
    return (char*)slot + slot_redzone2_offset(info);
}

static void slot_init(struct slot* slot, struct object_info info) {
    slot->magic = EMPTY_MAGIC;
    slot->next = 0;
    memset(slot_redzone1(slot), REDZONE_FILL, slot_redzone1_size(info.align));
    memset(slot_payload(slot, info.align), UNUSED_FILL, info.size);
    memset(slot_redzone2(slot, info), REDZONE_FILL, slot_redzone2_size(info));
}

static bool slot_check_redzone(struct slot* slot, struct object_info info) {
    return memchr_not(slot_redzone1(slot), REDZONE_FILL, slot_redzone1_size(info.align)) == NULL
        && memchr_not(slot_redzone2(slot, info), REDZONE_FILL, slot_redzone2_size(info)) == NULL;
}

static bool slot_check_unused(struct slot* slot, struct object_info info) {
    return memchr_not(slot_payload(slot, info.align), UNUSED_FILL, info.size) == NULL;
}

static void slot_write_unused(struct slot* slot, struct object_info info, int b) {
    memset(slot_payload(slot, info.align), b, info.size);
}

static void slot_on_alloc(struct slot* slot, struct object_info info) {
    assert(slot->magic == EMPTY_MAGIC && slot->next == 0, "slab is poisoned");
    assert(slot_check_redzone(slot, info), "redzone is corrupted");
    assert(slot_check_unused(slot, info), "slab is poisoned");
    slot->magic = OBJECT_MAGIC;
    slot_write_unused(slot, info, 0);
}

static void slot_on_dealloc(struct slot* slot, struct object_info info) {
    assert(slot->magic == OBJECT_MAGIC && slot->next == 0, "try to deallocate an object that is not allocated");
    assert(slot_check_redzone(slot, info), "redzone is corrupted");
    slot->magic = EMPTY_MAGIC;
    slot_write_unused(slot, info, UNUSED_FILL);
}

static struct page* page_from_slot(struct slot* slot) {
    const uintptr_t raw = (uintptr_t)slot;
    return (struct page*)(raw & ~(SLAB_PAGE - 1));
}

static size_t page_object_offset(size_t object_align) {
    return align_ceil(sizeof(struct page), slot_alignof(object_align));
}

static bool object_info_is_valid(struct object_info info) {
    return page_object_offset(info.align) + slot_sizeof(info) <= SLAB_PAGE;
}

static void page_init(struct page* page, struct object_info info) {
    size_t offset = page_object_offset(info.align);

    page->free_index = (uint16_t)offset;
    page->alloc_count = 0;

    while (1) {
        struct slot* const slot = (struct slot*)((char*)page + offset);
        slot_init(slot, info);

        const size_t slot_size = slot_sizeof(info);
        const size_t next_offset = offset + slot_size;
        if (next_offset + slot_size < SLAB_PAGE) {
            slot->next = (uint16_t)next_offset;
            offset = next_offset;
        } else {
            slot->next = 0;
            break;
        }
    }
}

static struct slot* page_pop_front(struct page* page, struct object_info info, bool* full) {
    assert(page->free_index != 0, "slab is corrupted: try to pop object from an fully-allocated page");

    struct slot* const front = (struct slot*)((char*)page + page->free_index);

    if (front->next == 0) {
        page->free_index = 0;
        *full = true;
    } else {
        page->free_index = front->next;
        *full = false;
    }

    front->next = 0;
    page->alloc_count++;
    return front;
}

static void page_push_front(struct page* page, struct slot* slot) {
    slot->next = page->free_index;
    page->free_index = (uint16_t)((uintptr_t)slot - (uintptr_t)page);
    page->alloc_count--;
}

void slab_init(struct slab_allocator* slab, size_t size, size_t align, const struct slab_page_allocator* pa) {
    assert(object_info_is_valid((struct object_info){ .size = size, .align = align }));

    linkedlist_init(&slab->partial_list);
    slab->page_allocator = *pa;
    slab->object_size = size;
    slab->object_align = align;
}

static struct page* slab_alloc_page(struct slab_allocator* slab) {
    struct object_info info = { .size = slab->object_size, .align = slab->object_align };

    struct page* page = slab->page_allocator.alloc(slab->page_allocator.ctx);
    if (!page) {
        return NULL;
    }

    page_init(page, info);
    linkedlist_init(&slab->partial_list);   // clear
    linkedlist_push_front(&slab->partial_list, &page->link);
    return page;
}

void* slab_alloc(struct slab_allocator* slab) {
    struct object_info info = { .size = slab->object_size, .align = slab->object_align };

    struct page* page;
    if (linkedlist_is_empty(&slab->partial_list)) {
        page = slab_alloc_page(slab);
        if (!page) {
            return NULL;
        }
    } else {
        struct linkedlist_link* const head = linkedlist_head(&slab->partial_list);
        page = objectof(head, struct page, link);
    }

    bool full;
    struct slot* slot = page_pop_front(page, info, &full);
    if (full) {
        linkedlist_remove(&page->link);
    }

    slot_on_alloc(slot, info);
    return slot_payload(slot, info.align);
}

void slab_dealloc(struct slab_allocator* slab, void* ptr) {
    struct object_info info = { .size = slab->object_size, .align = slab->object_align };

    struct slot* slot = (struct slot*)((char*)ptr - slot_payload_offset(info.align));
    slot_on_dealloc(slot, info);

    struct page* page = page_from_slot(slot);
    const bool was_full = page->free_index == 0;
    page_push_front(page, slot);

    if (page->alloc_count == 0) {
        if (!was_full) {
            linkedlist_remove(&page->link);
        }
        slab->page_allocator.dealloc(slab->page_allocator.ctx, page);
    } else if (was_full) {
        linkedlist_push_back(&slab->partial_list, &page->link);
    }
}
