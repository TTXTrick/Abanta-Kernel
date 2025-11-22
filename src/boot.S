; src/boot.S — NASM syntax (NOT GAS)
; Compiles with: nasm -f elf64 src/boot.S -o build/boot.o
; Simple 64-bit entry point for an ELF kernel loaded by GRUB.
; Calls kernel_main(), then halts.

BITS 64

global _start

section .text

_start:
    ; Clear rbp (stack frame base)
    xor     rbp, rbp

    ; Load stack pointer
    lea     rsp, [rel stack_top]

    ; Align stack to 16 bytes
    and     rsp, -16

    ; Call kernel C entry point
    extern  kernel_main
    call    kernel_main

.hang:
    cli
    hlt
    jmp     .hang


section .bss
align 16
stack:
    resb 16384       ; 16 KiB stack
stack_top:
