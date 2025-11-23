; src/boot.S — GRUB Multiboot v1 + 64-bit kernel jump
; NASM syntax

BITS 32

SECTION .multiboot
align 4
MULTIBOOT_MAGIC    equ 0x1BADB002
MULTIBOOT_FLAGS    equ 0
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

; Multiboot header GRUB needs
dd MULTIBOOT_MAGIC
dd MULTIBOOT_FLAGS
dd MULTIBOOT_CHECKSUM

SECTION .text
global _start

_start:
    ; GRUB loads this in 32-bit protected mode, paging OFF.

    ; Disable interrupts
    cli

    ; Load our GDT
    lgdt [gdt_ptr]

    ; Long-mode enable sequence:

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Load PML4 address (we identity map first 1GiB below)
    mov eax, pml4_table
    mov cr3, eax

    ; Enable long mode (IA32_EFER.LME)
    mov ecx, 0xC0000080      ; MSR for EFER
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging (and protected mode already enabled)
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Far jump into 64-bit code segment
    jmp 0x08:long_mode_entry


; ---------------------------------------------------------
; 64-BIT MODE
; ---------------------------------------------------------
BITS 64
long_mode_entry:
    ; set up stack
    lea rsp, [rel stack_top]
    and rsp, -16

    ; call kernel_main()
    extern kernel_main
    call kernel_main

.hang:
    cli
    hlt
    jmp .hang


; ---------------------------------------------------------
; PAGING STRUCTURES (identity map first 1GiB)
; ---------------------------------------------------------
SECTION .bss
align 4096

pml4_table:
    dq pml4e0

pml4e0:
    dq pdpt_table | 0x03     ; present + writable

align 4096
pdpt_table:
    dq pde_table | 0x03
    dq 0,0,0,0,0,0,0,0

align 4096
pde_table:
    ; 1GiB of 2MiB pages
    %assign i 0
    %rep 512
        dq (i*0x200000) | 0x83 ; present, write, huge page
        %assign i i+1
    %endrep


; ---------------------------------------------------------
; GDT
; ---------------------------------------------------------
SECTION .data

gdt64:
    dq 0                      ; null descriptor
    dq 0x00AF9A000000FFFF     ; 0x08: 64-bit code segment
    dq 0x00AF92000000FFFF     ; 0x10: data segment

gdt_ptr:
    dw gdt64_end - gdt64 - 1
    dq gdt64

gdt64_end:

; ---------------------------------------------------------
; stack
; ---------------------------------------------------------
SECTION .bss
align 16
stack:
    resb 16384
stack_top:
