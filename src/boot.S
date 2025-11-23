; ------------------------------------------------------------
; Minimal Multiboot1 header (valid, no illegal NASM operators)
; ------------------------------------------------------------

SECTION .multiboot
align 4

MULTIBOOT_MAGIC      equ 0x1BADB002
MULTIBOOT_FLAGS      equ 0x00000000        ; no flags required
MULTIBOOT_CHECKSUM   equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

; Multiboot header must be 3 consecutive 32-bit values

    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM


; ------------------------------------------------------------
; 32-bit entry point (GRUB jumps here)
; ------------------------------------------------------------

SECTION .text
align 4
global _start

_start:
    cli

    ; Load a GDT or switch to Long Mode later (optional for now)
    ; Just hang for now so GRUB knows we're alive

hang:
    hlt
    jmp hang
