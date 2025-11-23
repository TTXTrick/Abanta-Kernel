OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
  /* Put the multiboot header and early stub in the first page */
  . = 0x100000;
  .text :
  {
    *(.multiboot2_hdr)  /* header section from boot.S */
    *(.text*)
  }

  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss  : { *(COMMON) *(.bss*) }
}
