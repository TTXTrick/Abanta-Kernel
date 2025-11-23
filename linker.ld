/* linker.ld — linker script for a simple 64-bit freestanding kernel */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
  . = 0x100000; /* load at 1MB */
  .text : {
    *(.multiboot)
    *(.text)
    *(.rodata)
  }

  .data : {
    *(.data)
  }

  .bss : {
    __bss_start = .;
    *(.bss)
    *(COMMON)
    __bss_end = .;
  }

  /DISCARD/ : {
    *(.comment)
    *(.note*)
  }
}
