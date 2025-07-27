OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x200000;
    __higher_half_displacement = 0xffff800000000000 - .;

    .startup ALIGN(16) :
    {
        KEEP(*(.multiboot))
        KEEP(*(.startup))
    }

    . += __higher_half_displacement;

    .text ALIGN(16) : AT(ADDR(.text) - __higher_half_displacement)
    {
        *(.text)
        *(.stub .text.* .gnu.linkonce.t.*)
    }

    .rodata ALIGN(16) : AT(ADDR(.rodata) - __higher_half_displacement)
    {
        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.rodata1)
    }

    .data ALIGN(16) : AT(ADDR(.data) - __higher_half_displacement)
    {
        *(.data .data.* .gnu.linkonce.d.*)
        *(.data1)
    }

    .bss ALIGN(16) : AT(ADDR(.bss) - __higher_half_displacement)
    {
        *(COMMON)
        *(.bss .bss.* .gnu.linkonce.b.*)
        . = ALIGN(4K);
        __tmp_page = . - __higher_half_displacement;
        . += 0x13000;
    }

    . = 0xffff80000f000000;
    __stack_bottom = .;
    __stack_bottom_lba = 0x00600000;
    __stack_displacement = . - __stack_bottom_lba;

    .stack ALIGN(4K) (NOLOAD) : AT(ADDR(.stack) - __stack_displacement)
    {
        . += 0x00200000;
        __stack_top = .;
        __stack_top_lba = . - __stack_displacement;
    }
}
