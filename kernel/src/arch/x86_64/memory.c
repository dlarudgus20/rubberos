#include <stdalign.h>
#include <freec/string.h>
#include <freec/assert.h>

#include "arch/memory.h"
#include "arch/inst.h"
#include "memory.h"
#include "boot.h"

extern char __stack_bottom_lba[];
#define KSTACK_START_PHYS ((uintptr_t)__stack_bottom_lba)

static alignas(PAGE_SIZE) pagetable_t g_pagetable;

const char* mmap_entry_type_str(mmap_entry_type type) {
    switch (type) {
        case MMAP_ENTRY_AVAILABLE:
            return "Available";
        case MMAP_ENTRY_RESERVED:
            return "Reserved";
        case MMAP_ENTRY_ACPI_RECLAIMABLE:
            return "AcpiReclaimable";
        case MMAP_ENTRY_ACPI_NVS:
            return "AcpiNvs";
        case MMAP_ENTRY_BADRAM:
            return "BadRAM";
        default:
            return 0;
    }
}

static uintptr_t virt_to_phys_kernel(uintptr_t virt) {
    return virt - KERNEL_START_VIRT + KERNEL_START_PHYS;
}

static uintptr_t phys_to_virt_kernel(uintptr_t phys) {
    return phys - KERNEL_START_PHYS + KERNEL_START_VIRT;
}

static uintptr_t virt_to_phys_dynmem(uintptr_t virt, const struct mmap* mmap_dyn) {
    uintptr_t offset = virt - DYNMEM_START_VIRT;
    uintptr_t sum = 0;
    for (uint32_t i = 0; i < mmap_dyn->len; i++) {
        const struct mmap_entry* entry = mmap_dyn->entries + i;
        uintptr_t end_offset = sum + entry->length;

        if (offset < end_offset) {
            return entry->base + (offset - sum);
        }

        sum = end_offset;
    }
    panic("dynmem: invalid virtual address");
}

static uintptr_t phys_to_virt_dynmem(uintptr_t phys, const struct mmap* mmap_dyn) {
    uintptr_t sum = 0;
    for (uint32_t i = 0; i < mmap_dyn->len; i++) {
        const struct mmap_entry* entry = mmap_dyn->entries + i;

        if (entry->base <= phys && phys < entry->base + entry->length) {
            return DYNMEM_START_VIRT + sum + (phys - entry->base);
        }

        sum += entry->length;
    }
    panic("dynmem: invalid physical address");
}

static uintptr_t phys_to_virt(uintptr_t phys, const struct mmap* mmap_dyn) {
    if (phys >= mmap_dyn->entries[0].base) {
        return phys_to_virt_dynmem(phys, mmap_dyn);
    } else if (KERNEL_START_PHYS <= phys && phys < KERNEL_START_PHYS + KERNEL_SIZE) {
        return phys_to_virt_kernel(phys);
    } else {
        panic("dynmem: invalid physical address");
    }
}

static void pagetable_construct_kernel(void) {
    static alignas(PAGE_SIZE) pagetable_t pdpt = { 0 };
    static alignas(PAGE_SIZE) pagetable_t pdt = { 0 };
    g_pagetable[0x1ff] = virt_to_phys_kernel((uintptr_t)pdpt) | KERNEL_PAGE_FLAG;
    pdpt[0x1fe] = virt_to_phys_kernel((uintptr_t)pdt) | KERNEL_PAGE_FLAG;
    pdt[0] = KERNEL_START_PHYS | PAGE_FLAG_HUGE | KERNEL_PAGE_FLAG;
    pdt[1] = (KERNEL_START_PHYS + 0x00200000) | PAGE_FLAG_HUGE | KERNEL_PAGE_FLAG;
    pdt[0x78] = KSTACK_START_PHYS | PAGE_FLAG_HUGE | KERNEL_PAGE_FLAG;

    // TODO
    static alignas(PAGE_SIZE) pagetable_t pdt_fb = { 0 };
    uintptr_t fb_addr_aligned = bootinfo_get()->fb_addr_phys & ~0x001fffff;
    pdpt[0] = virt_to_phys_kernel((uintptr_t)pdt_fb) | KERNEL_PAGE_FLAG;
    for (int i = 0; i < PAGETABLE_LENGTH; i++) {
        pdt_fb[i] = fb_addr_aligned | PAGE_FLAG_HUGE | KERNEL_PAGE_FLAG;
        fb_addr_aligned += 0x00200000;
    }
    ((struct bootinfo*)bootinfo_get())->fb_info.addr = (volatile void*)(0xffffff8000000000 + (fb_addr_aligned & 0x001fffff));

    pagetable_set(virt_to_phys_kernel((uintptr_t)g_pagetable));
}

static void pagetable_construct_dyn_3(uintptr_t pdpt0_phys, uintptr_t pdt0_phys, uintptr_t pt0_phys) {
    // map first 3 pages of dynmem for dynpage_factory

    // temporarily map first 3 pages to PML4:1, PDPT:0, PDT:1, PT:[2,4] by recursive paging
    alignas(PAGE_SIZE) pagetable_t tmptable;
    uintptr_t tmp_phys = (uintptr_t)&tmptable - KSTACK_START_VIRT + KSTACK_START_PHYS;
    g_pagetable[1] = tmp_phys | KERNEL_PAGE_FLAG;
    tmptable[0] = tmp_phys | KERNEL_PAGE_FLAG;
    tmptable[1] = tmp_phys | KERNEL_PAGE_FLAG;
    tmptable[2] = pdpt0_phys | KERNEL_PAGE_FLAG;
    tmptable[3] = pdt0_phys | KERNEL_PAGE_FLAG;
    tmptable[4] = pt0_phys | KERNEL_PAGE_FLAG;
    tlb_flush_all();

    // construct geniune first 3 pages
    pagetable_t* pdpt0 = (pagetable_t*)(1ul << 39 | 1ul << 21 | 2ul << 12);
    pagetable_t* pdt0 = pdpt0 + 1;
    pagetable_t* pt0 = pdpt0 + 2;
    // DYNMEM_START_VIRT = PML4:0, PDPT:0, PDT:1, PT:0
    g_pagetable[0] = pdpt0_phys | KERNEL_PAGE_FLAG;
    (*pdpt0)[0] = pdt0_phys | KERNEL_PAGE_FLAG;
    (*pdt0)[1] = pt0_phys | KERNEL_PAGE_FLAG;
    (*pt0)[0] = pdpt0_phys | KERNEL_PAGE_FLAG;
    (*pt0)[1] = pdt0_phys | KERNEL_PAGE_FLAG;
    (*pt0)[2] = pt0_phys | KERNEL_PAGE_FLAG;

    // remove temporary page
    g_pagetable[1] = 0;
}

struct dynpage_factory {
    pagetable_t* next_table;
    pagetable_t* tables[4];
    uintptr_t first_3[3];
    uint16_t next_indices[4];
    uint32_t count;
    uint32_t first_3_index;
};

static struct dynpage_factory dynpage_factory_create(void) {
    pagetable_t* pdpt0 = (pagetable_t*)DYNMEM_START_VIRT;
    return (struct dynpage_factory){
        .next_table = pdpt0 + 3,
        .tables = { &g_pagetable, pdpt0, pdpt0 + 1, pdpt0 + 2 },
        .first_3 = { 0 },
        // pagetable_construct_dyn_3 maps PML4:0, PDPT:0, PDT:1, PT:[0,2]
        // thus next_indices is set by [1, 1, 2, 3]
        .next_indices = { 1, 1, 2, 3 },
        .count = 0,
        .first_3_index = 0,
    };
}

static void dynpage_factory_next_recur(struct dynpage_factory* factory,
    uint32_t level, uintptr_t addr_phys, const struct mmap* mmap_dyn
) {
    assert(!(level == 0 && factory->next_indices[level] >= 256), "dynpage_factory_next_recur() out of bound");

    if (factory->next_indices[level] >= PAGETABLE_LENGTH) {
        pagetable_t* next = factory->next_table;
        memset(next, 0, sizeof(pagetable_t));
        factory->tables[level] = next;
        factory->next_indices[level] = 0;
        factory->next_table++;

        uint32_t next_phys = virt_to_phys_dynmem((uintptr_t)next, mmap_dyn);
        dynpage_factory_next_recur(factory, level - 1, next_phys, mmap_dyn);
    }

    (*factory->tables[level])[factory->next_indices[level]] = addr_phys | KERNEL_PAGE_FLAG;
    factory->next_indices[level]++;
    factory->count++;
}

static void dynpage_factory_next(struct dynpage_factory* factory,
    uintptr_t addr_phys, const struct mmap* mmap_dyn
) {
    if (factory->first_3_index < 3) {
        factory->first_3[factory->first_3_index] = addr_phys;
        if (++factory->first_3_index == 3) {
            pagetable_construct_dyn_3(factory->first_3[0], factory->first_3[1], factory->first_3[2]);
            factory->count = 3;
        }
    } else {
        dynpage_factory_next_recur(factory, 3, addr_phys, mmap_dyn);
    }
}

static void pagetable_construct_dyn(const struct mmap* mmap_dyn) {
    struct dynpage_factory factory = dynpage_factory_create();

    for (size_t i = 0; i < mmap_dyn->len; i++) {
        const struct mmap_entry* entry = mmap_dyn->entries + i;
        for (uintptr_t addr_phys = entry->base;
            addr_phys < entry->base + entry->length;
            addr_phys += PAGE_SIZE
        ) {
            dynpage_factory_next(&factory, addr_phys, mmap_dyn);
        }
    }
}

void pagetable_construct(const struct mmap* mmap_dyn) {
    pagetable_construct_kernel();
    pagetable_construct_dyn(mmap_dyn);
    tlb_flush_all();
}

struct page_iterator {
    uint16_t pl4i, pdpi, pdti, pti;
    pagetable_t *pdpt, *pdt, *pt;
};

static struct page_iterator page_iterator_from_virt(uintptr_t virt) {
    uint16_t pl4i = (uint16_t)((virt & 0x0000ff8000000000) >> 39);
    uint16_t pdpi = (uint16_t)((virt & 0x0000007fc0000000) >> 30);
    uint16_t pdti = (uint16_t)((virt & 0x000000003fe00000) >> 21);
    uint16_t pti  = (uint16_t)((virt & 0x00000000001ff000) >> 12);
    return (struct page_iterator){ .pl4i = pl4i, .pdpi = pdpi, .pdti = pdti, pti = pti };
}

void pagetable_map(uintptr_t begin_virt, uintptr_t end_virt, uintptr_t phys, page_entry_t flags) {
    struct page_iterator it = page_iterator_from_virt(begin_virt);
    // TODO: page allocation required
    (void)it;
}

#include <stdbool.h>
#include <freec/inttypes.h>
#include "drivers/serial.h"

static void print_page_flags(page_entry_t entry) {
    bool printed = false;
#define FLAG(name) \
    if (entry & PAGE_FLAG_##name) { \
        if (!printed) printed = true; \
        else serial_printf(" | "); \
        serial_printf(#name); \
    }
    FLAG(PRESENT)
    FLAG(WRITABLE)
    FLAG(USER)
    FLAG(WRITE_THROUGH)
    FLAG(NO_CACHE)
    FLAG(ACCESSED)
    FLAG(DIRTY)
    FLAG(HUGE)
    FLAG(GLOBAL)
    FLAG(NO_EXECUTE)
#undef FLAG
    if (!printed) {
        serial_printf("0");
    }
}

static void pagetable_print_recur(pagetable_t* table, const struct mmap* mmap_dyn,
    const char* names[], unsigned depth, uintptr_t pagesize, uintptr_t virt
) {
    int leaf_begin = -1;
    for (unsigned idx = 0; idx <= PAGETABLE_LENGTH; idx++) {
        bool present = idx < PAGETABLE_LENGTH && ((*table)[idx] & PAGE_FLAG_PRESENT);
        bool leaf = present && (depth == 3 || ((*table)[idx] & PAGE_FLAG_HUGE));

        if (leaf_begin != -1) {
            uintptr_t prev = (*table)[idx - 1] & PAGE_MASK_ADDR;
            if (!leaf || prev + pagesize != ((*table)[idx] & PAGE_MASK_ADDR)) {
                unsigned shifts[] = { 39, 30, 21, 12 };

                uintptr_t phys = (*table)[leaf_begin] & PAGE_MASK_ADDR;
                uintptr_t len = ((uintptr_t)idx - leaf_begin) * pagesize;
                uintptr_t sign = 0xffff800000000000;
                uintptr_t v_raw = (virt << 9 | (uintptr_t)leaf_begin) << shifts[depth];
                uintptr_t v_ext = v_raw & sign ? v_raw | sign : v_raw;

                serial_printf("%s %#018"PRIxPTR"-%#018"PRIxPTR" to %#"PRIxPTR"-%#"PRIxPTR"\n", names[depth], v_ext, v_ext + len, phys, phys + len);
                leaf_begin = leaf ? (int)idx : -1;
            }
        }
        if (leaf) {
            if (leaf_begin == -1) {
                leaf_begin = (int)idx;
            }
            continue;
        }
        if (!present) {
            continue;
        }

        page_entry_t entry = (*table)[idx];
        uintptr_t phys = entry & PAGE_MASK_ADDR;

        serial_printf("%s %#7x to %#"PRIxPTR": ", names[depth], idx, phys);
        print_page_flags(entry);
        serial_putchar('\n');

        pagetable_t* subtable = (pagetable_t*)phys_to_virt(phys, mmap_dyn);
        pagetable_print_recur(subtable, mmap_dyn, names, depth + 1, pagesize >> 9, virt << 9 | idx);
    }
}

void pagetable_print_with_dyn(const struct mmap* mmap_dyn) {
    const char* names[] = { "PML4E", " PDPE", "  PDE", "   PT" };
    pagetable_print_recur(&g_pagetable, mmap_dyn, names, 0, 0x0000008000000000, 0);
}
