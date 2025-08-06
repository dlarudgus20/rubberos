#include <stdalign.h>
#include <freec/stdlib.h>
#include <freec/string.h>
#include <buddy/buddy.h>
#include <slab/slab.h>

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

void mmap_init(void) {
    construct_mmap_dyn(bootinfo_get()->mmap);
    pagetable_construct(&g_mmap_dyn.mmap);
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

void mem_pagetable_print(void) {
    pagetable_print(&g_mmap_dyn.mmap);
}
