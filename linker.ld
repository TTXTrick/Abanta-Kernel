/* Linker script for Abanta kernel (simple) */
OUTPUT_FORMAT("elf64-x86-64")
ENTRY(start)
SECTIONS
{
  . = 1M; /* place kernel at 1 MiB */
  .text : {
    *(.multiboot)
    *(.text.boot)
    *(.text*)
  }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : {
    __bss_start = .;
    *(.bss*)
    *(COMMON)
    __bss_end = .;
  }
}
