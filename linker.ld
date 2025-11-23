/* linker.ld — simple linker script for a 64-bit kernel ELF */
OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
  . = 0x100000; /* place kernel at 1MiB (typical for multiboot kernels) */

  .text : {
    *(.text)
    *(.text.*)
    *(.rodata)
    *(.rodata.*)
  }

  .data : {
    *(.data)
    *(.data.*)
  }

  .bss : {
    __bss_start = .;
    *(.bss)
    *(.bss.*)
    __bss_end = .;
  }

  /DISCARD/ : { *(.eh_frame) *(.comment) }
}
