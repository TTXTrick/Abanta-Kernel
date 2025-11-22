/* Linker script for Abanta kernel (ELF64) */
ENTRY(kernel_entry)

SECTIONS
{
  . = 0x100000; /* load at 1MiB (GRUB will honor ELF load addresses, but this is a conventional kernel base) */

  .text ALIGN(0x1000) : {
    *(.multiboot2_header)   /* put multiboot2 header here */
    *(.text)
  }

  .rodata ALIGN(0x1000) : { *(.rodata) }

  .data ALIGN(0x1000) : {
    *(.data)
    *(.bss)
    . = ALIGN(0x1000);
    PROVIDE(end = .);
  }
}
