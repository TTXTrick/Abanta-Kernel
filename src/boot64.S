; boot64.S - multiboot2 header (ELF64) + minimal long-mode bootstrap
; NASM 64-bit, assembled to ELF64.
; Minimal, correct Multiboot2 header (magic/arch/length/checksum + END tag).
; Calls kernel_main in C (must be provided by your kernel C file).

BITS 64
DEFAULT REL

section .multiboot2
    align 8
mb2_header:
    dd 0xE85250D6           ; multiboot2 magic
    dd 0                    ; architecture (0 = i386; GRUB uses ELF class to decide mode)
    dd mb2_header_end - mb2_header  ; full header length (magic..end tag)
    dd -(0xE85250D6 + 0 + (mb2_header_end - mb2_header)) ; checksum (so sum = 0 mod 2^32)

    ; --- END tag (required) ---
    dw 0    ; type = 0 (MB2_TAG_END)
    dw 0    ; flags (must be zero)
    dd 8    ; size of this tag (8)

mb2_header_end:

section .text
    align 16
    global _start
    extern kernel_main

_start:
    ; minimal sanity: disable interrupts and set up a stack then call C entry
    cli
    xor rbp, rbp

    ; set up a stack (stack_top label defined in .bss)
    lea rsp, [rel stack_top]

    ; ensure direction flag clear
    cld

    ; call kernel entry (C)
    call kernel_main

.halt_loop:
    hlt
    jmp .halt_loop

section .bss
    align 4096
stack_bottom:
    resb 16384            ; 16 KiB stack
stack_top:
