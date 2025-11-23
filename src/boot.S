; src/boot.S
; Minimal multiboot2 header then jump to C entry
BITS 64
SECTION .multiboot
align 8
MB2_HEADER:
    ; Multiboot2 header: magic, architecture, header_length, checksum
    ; magic 0xE85250D6, arch 0 (i386) or 3 (x86_64)? Multiboot2 uses arch=0 for i386; GRUB ignores arch mostly.
    ; We'll put a basic header with no tags (safe for GRUB).
    dd 0xE85250D6
    dd 0
    ; header length (we provide only these 12 bytes)
    dd 12
    dd -(0xE85250D6 + 0 + 12) ; checksum

SECTION .text
global _start
extern kernel_main

_start:
    ; GRUB will load the binary and then jump to _start
    ; simply jump to kernel_main (C entry)
    jmp kernel_main
