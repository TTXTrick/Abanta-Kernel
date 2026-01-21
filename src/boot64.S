; boot64.S
; Multiboot2 32-bit entry → switch to 64-bit long mode → call kernel_main

; =========================
; Multiboot2 header (32-bit)
; =========================

BITS 32 ; why not

section .multiboot2
align 8

mb2_start:
    dd 0xE85250D6            ; magic
    dd 0                     ; architecture (i386)
    dd mb2_end - mb2_start   ; header length
    dd -(0xE85250D6 + 0 + (mb2_end - mb2_start))

    ; END tag
    dw 0
    dw 0
    dd 8
mb2_end:

; =========================
; 32-bit entry point
; =========================

section .text
align 16
global _start
extern kernel_main

_start:
    cli
    mov esp, stack32_top

    ; load GDT
    lgdt [gdt_desc]

    ; enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; load PML4 base
    mov eax, pml4
    mov cr3, eax

    ; enable long mode
    mov ecx, 0xC0000080      ; IA32_EFER
    rdmsr
    or eax, 1 << 8           ; LME
    wrmsr

    ; enable paging
    mov eax, cr0
    or eax, 0x80000001       ; PG | PE
    mov cr0, eax

    ; far jump to 64-bit code
    jmp 0x08:long_mode_start

; =========================
; 64-bit code
; =========================

BITS 64

long_mode_start:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov rsp, stack64_top
    xor rbp, rbp
    cld

    call kernel_main

.halt:
    hlt
    jmp .halt

; =========================
; GDT
; =========================

section .rodata
align 8

gdt:
    dq 0x0000000000000000    ; null
    dq 0x00AF9A000000FFFF    ; code (0x08)
    dq 0x00AF92000000FFFF    ; data (0x10)

gdt_desc:
    dw gdt_desc - gdt - 1
    dq gdt

; =========================
; Paging (identity map first 2MB)
; =========================

section .bss
align 4096

pml4:
    resq 512
pdpt:
    resq 512
pd:
    resq 512

align 4096
stack32:
    resb 4096
stack32_top:

align 4096
stack64:
    resb 16384
stack64_top:

section .data
align 4096

    ; build page tables at link time
    ; pml4[0] -> pdpt
    ; pdpt[0] -> pd
    ; pd[0]   -> 2MB page

    dq pdpt | 0x3
pml4_end:

pdpt:
    dq pd | 0x3
pdpt_end:

pd:
    dq 0x00000000 | 0x83      ; 2MB page, RW | Present
