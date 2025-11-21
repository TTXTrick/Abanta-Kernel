ENTRY(_start)
SECTIONS
{
  . = 0x100000; /* load at 1MiB like before */
  .multiboot : { KEEP(*(.multiboot)) }
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
}
