OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x200000;

    .text BLOCK(4K) : ALIGN(4K)
    {
        KEEP(*(.multiboot))
        *(.text)
        *(.stub .text.* .gnu.linkonce.t.*)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.rodata1)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data .data.* .gnu.linkonce.d.*)
        *(.data1)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss .bss.* .gnu.linkonce.b.*)
    }
}
