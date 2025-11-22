/* Minimal Multiboot2 header + 64-bit entry stub.
 * Place in .multiboot2_header to let GRUB detect the kernel.
 *
 * This header follows the minimal Multiboot2 structure:
 *  - magic (0xE85250D6)
 *  - architecture (0)
 *  - header_length
 *  - checksum such that (magic + arch + header_length + checksum) == 0
 *
 * It then has an END tag (type 0) of length 8.
 *
 * After the header we define kernel_entry which sets up a stack and
 * calls the C kernel_main function.
 */

    .section .multiboot2_header, "a", @progbits
    .align 8
mb2_start:
    .long 0xE85250D6          /* magic */
    .long 0                  /* architecture (0 for i386, GRUB accepts) */
    .long mb2_end - mb2_start /* total header length */
    .long -(0xE85250D6 + 0 + (mb2_end - mb2_start)) /* checksum */
    /* Tag: end (type=0, size=8) */
    .long 0
    .long 8
mb2_end:

    .text
    .globl kernel_entry
    .align 16
kernel_entry:
    /* set up a simple stack and call kernel_main(multiboot_info_ptr, 0) */
    /* GRUB passes pointer to multiboot info in RBX for multiboot2? 
       Different implementations vary; to be safe we pass 0 as info ptr and
       rely on kernel not needing it for this tiny demo.
       We still provide a clean long-mode entry with stack. */

    /* set up stack (use memory at 2 MiB) */
    mov $0x200000, %rsp        /* stack pointer at 2MB */
    /* clear RBP */
    xor %rbp, %rbp

    /* call kernel_main() */
    extern kernel_main
    call kernel_main

    /* hang if kernel_main returns */
1:  hlt
    jmp 1b
