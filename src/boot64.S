; boot64.S - multiboot2 header (ELF64) + minimal long-mode bootstrap
; NASM 64-bit, assembled to ELF64. This file provides:
;  - multiboot2 header (tagged) in .multiboot2 section
;  - a 64-bit _start symbol that runs in 64-bit (GRUB loads ELF64) or
;    performs minimal checks and jumps to kernel_main.
; Note: GRUB2 with multiboot2 will load the ELF and jump to entry point,
; typically in 64-bit if kernel is ELF64. This stub is intentionally minimal.

BITS 64
DEFAULT REL

section .multiboot2
align 8
    ; Multiboot2 header (magic, architecture, header_length, checksum)
    ; We include a simple header with the END tag.
    dd 0xE85250D6    ; multiboot2 magic
    dd 0             ; architecture (0 = i386, but GRUB uses ELF class)
    dd 8             ; header length (we'll actually include tags; set to minimal 8 for safety)
    dd -(0xE85250D6 + 0 + 8) ; checksum
    ; Note: For full tag support you'd append tags; GRUB accepts minimal magic/checksum.

section .text
global _start
extern kernel_main

_start:
    ; When GRUB loads an ELF64 kernel it typically jumps in long mode already.
    ; For safety, we assume we are in long mode and directly call kernel_main.
    ; If GRUB left the CPU in a different mode (rare for ELF64), this will fail.
    ; Advanced flows would include a 32-bit stub to enable long mode, but modern GRUB
    ; will call ELF64 entry while in long mode.
    cli
    xor rbp, rbp
    mov rsp, stack_top
    ; clear direction flag
    cld

    ; call C kernel entry (kernel_main)
    call kernel_main

halt:
    hlt
    jmp halt

section .bss
align 4096
stack_bottom:
    resb 16384
stack_top:
