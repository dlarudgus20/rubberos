#include <freec/string.h>

#include "arch/x86_64/interrupt.h"
#include "arch/x86_64/inst.h"

#define GDT_FLAG_ACCESSED   0x01
#define GDT_FLAG_RW         0x02
#define GDT_FLAG_DC         0x04
#define GDT_FLAG_CODE       0x08
#define GDT_FLAG_TSS_AVAIL  0x09
#define GDT_FLAG_TSS_BUSY   0x0b

#define GDT_FLAG_USER       0x10
#define GDT_FLAG_DPL_3      0x60
#define GDT_FLAG_PRESENT    0x80

union __attribute__((packed)) gdt {
    struct __attribute__((packed)) {
        unsigned limit_low:16;
        unsigned base_low:24;
        unsigned flags1:8;
        unsigned limit_high:4;
        unsigned flags2:4;
        unsigned base_high:8;
    };
    uint64_t descriptor;
};

struct __attribute__((packed)) idt {
    unsigned offset_low:16;
    unsigned segment:16;
    unsigned ist:3;
    unsigned reserved1:5;
    unsigned type:4;
    unsigned reserved2:1;
    unsigned dpl:2;
    unsigned p:1;
    unsigned offset_mid:16;
    unsigned offset_high:32;
    unsigned reserved3:32;
};

static union gdt g_gdt[5];
static struct idt g_idt[256];
static struct tss g_tss_df;

static void init_gdt(union gdt* gdt, uint32_t base, uint32_t limit, uint8_t flags) {
    gdt->limit_low = limit & 0xffff;
    gdt->base_low = base & 0xffffff;
    gdt->flags1 = flags;
    gdt->limit_high = (limit >> 16) & 0xf;
    gdt->flags2 = 0xa;
    gdt->base_high = (base >> 24) & 0xff;
}

static void init_tss(union gdt* gdt, struct tss* tss, uint8_t flags) {
    init_gdt(gdt, (uint64_t)tss, sizeof(struct tss) - 1, flags);
    gdt[1].descriptor = (uint64_t)tss >> 32;
}

static void init_idt(struct idt* idt, uint16_t segment, void (*handler)(), uint8_t dpl, uint8_t ist) {
    idt->offset_low = (uint64_t)handler & 0xffff;
    idt->segment = segment;
    idt->ist = ist;
    idt->reserved1 = 0;
    idt->type = 0xe;
    idt->reserved2 = 0;
    idt->dpl = dpl;
    idt->p = 1;
    idt->offset_mid = ((uint64_t)handler >> 16) & 0xffff;
    idt->offset_high = ((uint64_t)handler >> 32) & 0xffffffff;
    idt->reserved3 = 0;
}

void interrupt_init(void) {
    memset(g_gdt, 0, sizeof(g_gdt));
    memset(g_idt, 0, sizeof(g_idt));
    memset(&g_tss_df, 0, sizeof(g_tss_df));

    static char df_stack[0x2000];
    g_tss_df.ist[0] = (uint64_t)(df_stack + sizeof(df_stack));

    init_gdt(g_gdt + 1, 0, 0xffffffff, GDT_FLAG_PRESENT | GDT_FLAG_USER | GDT_FLAG_RW | GDT_FLAG_CODE);
    init_gdt(g_gdt + 2, 0, 0xffffffff, GDT_FLAG_PRESENT | GDT_FLAG_USER | GDT_FLAG_RW);
    init_tss(g_gdt + 3, &g_tss_df, GDT_FLAG_PRESENT | GDT_FLAG_TSS_AVAIL);
    load_gdt(g_gdt, sizeof(g_gdt));
    load_tss(0x18);

    for (size_t i = 0; i < sizeof(g_idt) / sizeof(g_idt[0]); i++) {
        init_idt(g_idt + i, 0x08, isr_unknown, 0, 0);
    }
    init_idt(g_idt + 0, 0x08, isr_divide_by_zero, 0, 0);
    init_idt(g_idt + 1, 0x08, isr_debug, 0, 0);
    init_idt(g_idt + 2, 0x08, isr_nmi, 0, 0);
    init_idt(g_idt + 3, 0x08, isr_breakpoint, 0, 0);
    init_idt(g_idt + 4, 0x08, isr_overflow, 0, 0);
    init_idt(g_idt + 5, 0x08, isr_bound_range_exceeded, 0, 0);
    init_idt(g_idt + 6, 0x08, isr_invalid_opcode, 0, 0);
    init_idt(g_idt + 7, 0x08, isr_device_not_available, 0, 0);
    init_idt(g_idt + 8, 0x08, isr_double_fault, 0, 1);
    init_idt(g_idt + 10, 0x08, isr_invalid_tss, 0, 0);
    init_idt(g_idt + 11, 0x08, isr_segment_not_present, 0, 0);
    init_idt(g_idt + 12, 0x08, isr_stack_segment_fault, 0, 0);
    init_idt(g_idt + 13, 0x08, isr_general_protection_fault, 0, 0);
    init_idt(g_idt + 14, 0x08, isr_page_fault, 0, 0);
    init_idt(g_idt + 16, 0x08, isr_x87_floating_point, 0, 0);
    init_idt(g_idt + 17, 0x08, isr_alignment_check, 0, 0);
    init_idt(g_idt + 18, 0x08, isr_machine_check, 0, 0);
    init_idt(g_idt + 19, 0x08, isr_simd_floating_point, 0, 0);
    load_idt(g_idt, sizeof(g_idt));
}
