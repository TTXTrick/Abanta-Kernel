/* 64-bit kernel linker script */

ENTRY(_start)

SECTIONS
{
    /* Load at 1MB to avoid BIOS memory areas */
    . = 1M;

    /* Multiboot header MUST be in the first 8KB */
    .multiboot :
    {
        KEEP(*(.multiboot))
        KEEP(*(.multiboot_header))
    }

    .text ALIGN(4K) :
    {
        *(.text*)
        *(.rodata*)
    }

    .data ALIGN(4K) :
    {
        *(.data*)
    }

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss*)
        . = ALIGN(4K);
    }

    /* Kernel end marker */
    _kernel_end = .;
}
