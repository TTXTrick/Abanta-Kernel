/* src/boot.S - GAS (GNU assembler) syntax, 64-bit entry stub for an ELF64 kernel
 *
 * - Assembles with: gcc -c src/boot.S -m64
 * - Linkable into an ELF64 kernel where the C entry is `kernel_main`.
 * - Sets up a simple stack and calls kernel_main.
 *
 * Notes:
 * - GRUB (or your bootloader) must load the ELF and jump to _start in 64-bit mode.
 * - kernel_main should be provided in C (or asm) and follow the SysV AMD64 calling convention:
 *      void kernel_main(void);
 *
 * - If you need a multiboot header you should add it (multiboot2 header) in a separate section.
 *   This file purposely stays minimal to avoid complexity.
 */

    .section .text
    .globl  _start
    .type   _start, @function

_start:
    /* Clear frame pointer (makes backtraces sensible and consistent) */
    xorq    %rbp, %rbp

    /* Load stack pointer to top of our stack (stack grows down).
       We use RIP-relative addressing so this file is position-independent. */
    leaq    stack_top(%rip), %rsp

    /* Align stack to 16 bytes before calling C code (ABI requirement) */
    andq    $-16, %rsp

    /* Call the C kernel entry point. Provide no args (kernel_main must be void kernel_main(void)). */
    call    kernel_main

/* If kernel_main returns, halt the CPU in a tight loop. */
.halt_loop:
    cli                     /* disable interrupts */
    hlt
    jmp     .halt_loop

    .size _start, .-_start

/* ------------------------------
   Simple stack (BSS style)
   16 KiB stack, aligned
   ------------------------------ */
    .section .bss
    .align 16
stack:
    .skip 16384             /* reserve 16 KiB */
stack_top:
