OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
  . = 0x200000; /* place kernel at 2MiB (conservative) */

  /* Multiboot2 header section must be in the first 8KiB per spec when GRUB searches,
     but placing the header in boot64.S in its own section is fine. */
  .multiboot2 ALIGN(8) : {
    KEEP(*(.multiboot2))
  }

  .text ALIGN(8) : {
    *(.text*)
    *(.rodata*)
  }

  .data ALIGN(8) : {
    *(.data*)
  }

  .bss ALIGN(8) : {
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    __bss_end = .;
  }
}
