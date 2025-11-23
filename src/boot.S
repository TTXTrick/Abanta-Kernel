; src/boot.S — NASM syntax
; Assemble with: nasm -f elf64 src/boot.S -o build/boot.o
; Simple 64-bit entry that sets up stack and calls kernel_main.

BITS 64

global _start

section .text

_start:
    ; Clear rbp
    xor     rbp, rbp

    ; Setup a simple stack
    lea     rsp, [rel stack_top]
    and     rsp, -16         ; align to 16

    ; Call C entry point
    extern  kernel_main
    call    kernel_main

.hang:
    cli
    hlt
    jmp     .hang

section .bss
align 16
stack:
    resb 16384        ; 16 KiB stack
stack_top:
