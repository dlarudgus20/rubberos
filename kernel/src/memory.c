#include "memory.h"
#include "boot.h"

#include "drivers/serial.h"

static const char* mmap_entry_type_str(mmap_entry_type type) {
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

void mmap_print(void) {
    const struct mmap* mmap = bootinfo_get()->mmap;
    serial_printf("Memory Map: %d entries\n", mmap->len);
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
