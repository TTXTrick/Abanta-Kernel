; Minimal 64-bit entry with Multiboot2 header (so GRUB can load).
; This file provides a small multiboot2 header and an _start label that sets up a stack
; and calls kernel_main (C).

BITS 64
SECTION .multiboot2_hdr
ALIGN 8

; Multiboot2 header (simple, no optional tags). Magic and checksum required.
; magic: 0xE85250D6
; architecture: 0 (i386) but GRUB supports 64-bit ELF if the kernel itself is ELF64.
MB2_MAGIC      equ 0xE85250D6
MB2_ARCH       equ 0
; header length (we place 8 bytes header + end tag = 16)
MB2_LENGTH     equ 16
MB2_CHECKSUM   equ -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)

dd MB2_MAGIC
dd MB2_ARCH
dd MB2_LENGTH
dd MB2_CHECKSUM

; end tag for multiboot2 header
dd 0 ; end tag type
dd 8 ; end tag size

SECTION .text
global _start
extern kernel_main

_start:
    ; set up a minimal stack (use some high address in virtual memory)
    mov     rbp, 0
    mov     rsp, stack_top

    ; zero out registers commonly expected
    xor     rax, rax
    xor     rbx, rbx
    xor     rcx, rcx
    xor     rdx, rdx

    ; Call C entry point
    call    kernel_main

halt:
    cli
    hlt
    jmp halt

SECTION .bss
    ALIGN 16
stack_bottom:
    ; reserve 16KB stack
    resb 16384
stack_top:
