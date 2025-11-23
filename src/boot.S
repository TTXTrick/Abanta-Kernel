; =====================================================================
;   Minimal Working Multiboot2 Bootloader for 64-bit Kernel
; =====================================================================

BITS 32
SECTION .multiboot2
align 8

multiboot_header:
    dd 0xe85250d6              ; magic
    dd 0                       ; architecture = i386
    dd (multiboot_header_end - multiboot_header) ; header length
    dd 0x100000000 - (0xe85250d6 + 0 + \
        (multiboot_header_end - multiboot_header)) ; checksum

    ; End tag
    dw 0                       ; type = 0 (END TAG)
    dw 0                       ; flags
    dd 8                       ; size = 8

multiboot_header_end:

; =====================================================================
; GRUB jumps here (32-bit protected mode)
; =====================================================================

SECTION .text
global _start
_start:
    mov esp, stack_top         ; set a simple stack

    ; GRUB passes (eax=magic, ebx=multiboot2 struct)
    push ebx
    push eax

    call switch_to_long_mode

.hang:
    jmp .hang   ; never return

; =====================================================================
; Set up identity paging & enter long mode
; =====================================================================

switch_to_long_mode:
    cli

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Load PML4
    mov eax, pml4_table
    mov cr3, eax

    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Far jump → 64-bit mode
    jmp 0x08:long_mode_entry

; =====================================================================
; 64-bit code starts here
; =====================================================================

BITS 64
long_mode_entry:
    mov rsp, stack_top_64

    extern kernel_main
    call kernel_main

halt:
    hlt
    jmp halt

; =====================================================================
; Paging structures (identity map first 2 MB)
; =====================================================================

SECTION .bss
align 4096
pml4_table:
    dq pdpt_table | 0x03
pdpt_table:
    dq pd_table | 0x03
    times 511 dq 0
pd_table:
    dq 0x00000000 | 0x83   ; 2 MB page
    times 511 dq 0

; =====================================================================
; Stack
; =====================================================================

align 16
stack_space: resb 4096
stack_top equ stack_space + 4096

SECTION .bss
align 16
stack_space_64: resb 4096
stack_top_64 equ stack_space_64 + 4096
