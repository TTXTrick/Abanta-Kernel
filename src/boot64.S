; boot64.S — Multiboot2 32→64 bit boot stub (NASM, ELF64)
; GRUB loads us in 32-bit protected mode

BITS 32 ; why not
DEFAULT REL

section .multiboot
align 8

mb2_start:
    dd 0xE85250D6          ; Multiboot2 magic
    dd 0                  ; architecture (i386)
    dd mb2_end - mb2_start
    dd -(0xE85250D6 + 0 + (mb2_end - mb2_start))

    ; END tag
    dw 0
    dw 0
    dd 8
mb2_end:

section .text
align 16
global _start
extern kernel_main

_start:
    cli
    mov esp, stack_top32

    ; enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; load PML4
    mov eax, pml4
    mov cr3, eax

    ; enable long mode
    mov ecx, 0xC0000080      ; IA32_EFER
    rdmsr
    or eax, 1 << 8           ; LME
    wrmsr

    ; enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; load 64-bit GDT
    lgdt [gdt64_ptr]

    ; far jump to long mode
    jmp 0x08:long_mode_start


; ---------------------
; 64-bit mode
; ---------------------
BITS 64

long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    lea rsp, [stack_top64]

    call kernel_main

.hang:
    hlt
    jmp .hang


; ---------------------
; Page tables (identity map first 2 MiB)
; ---------------------
align 4096
pml4:
    dq pdpt + 0x003          ; Present | Writable

align 4096
pdpt:
    dq pd + 0x003            ; Present | Writable

align 4096
pd:
    dq 0x00000000 + 0x083    ; Present | Writable | 2MB
    times 511 dq 0

; ---------------------
; GDT
; ---------------------
align 8
gdt64:
    dq 0
    dq 0x00AF9A000000FFFF    ; code
    dq 0x00AF92000000FFFF    ; data

gdt64_ptr:
    dw gdt64_ptr - gdt64 - 1
    dq gdt64


; ---------------------
; Stacks
; ---------------------
section .bss
align 16

stack_bottom32:
    resb 4096
stack_top32:

stack_bottom64:
    resb 16384
stack_top64:
