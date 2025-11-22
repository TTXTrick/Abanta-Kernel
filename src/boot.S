; Minimal boot code that contains a Multiboot2 info header (so GRUB recognizes it)
; then performs CPU setup and enters long mode, then jumps to C entry 'kernel_main'.
; NASM (elf64) syntax.

BITS 64
global start
extern kernel_main

; --- Multiboot2 header (in .multiboot section) ---
section .multiboot
align 8
; magic and flags per multiboot2 spec — simple header setting
dd 0xE85250D6            ; magic
dd 0                     ; architecture, checksum placeholder (not needed here)
dd 0x00000002            ; header flags (minimum)
dd 0                     ; header checksum (ignored by GRUB if flags minimal)

; NOTE: Many multiboot2 features are ignored here. This header is intentionally minimal
; to allow GRUB to detect and load the kernel ELF.

section .text.boot
align 16
start:
    ; We expect GRUB to have loaded the kernel and left CPU in protected or long mode
    ; Many GRUB setups will jump to kernel with unsupported state; for safety, we re-init
    ; a minimal protected-mode -> long-mode switch sequence here.
    ; For brevity we assume CPU can be switched from whatever state into long mode.
    ; We'll set up a very small 4-level page table (identity map low 2MB + higher map).
    cli

    ; load GDT for long mode
    lgdt [gdt_descriptor]

    ; enable PAE and set up PML4 etc.
    ; allocate page tables inline in code (they reside in .bss area below).
    mov rax, cr4
    or rax, 1 << 5        ; set PAE (bit 5)
    mov cr4, rax

    ; set up page tables base: use the page tables created in memory below
    lea rax, [pml4_table]
    mov cr3, rax

    ; enable long mode via EFER
    mov ecx, 0xC0000080   ; MSR_EFER
    rdmsr
    or eax, 0x00000100    ; set LME (bit 8) in EFER
    wrmsr

    ; enable paging (set PG bit in CR0)
    mov rax, cr0
    or rax, 0x80000000
    mov cr0, rax

    ; far jump to 64-bit code segment selector
    jmp 0x08:long_mode_entry

; 64-bit entry
long_mode_entry:
    ; set up data segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; call C kernel entry
    extern kernel_main
    call kernel_main

    hlt
    jmp $

; ----------------------------
; simple GDT (null, code, data)
; ----------------------------
section .data
align 8
gdt_start:
    dq 0x0000000000000000
    dq 0x00AF9A000000FFFF    ; code: base=0, limit=0xFFFFF, long-mode compatible
    dq 0x00AF92000000FFFF    ; data
gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dq gdt_start

; ----------------------------
; Very minimal page tables — identity map 0-2MB and map kernel at 1MB
; We'll allocate PML4, PDPT, PD and mark present.
; ----------------------------
align 4096
pml4_table:
    dq pdpt_table | 3

align 4096
pdpt_table:
    dq pd_table | 3

align 4096
pd_table:
    ; 2MB page at 0 (maps first 2MB)
    dq 0x00000000 | 0x83  ; present, RW, 2MB (PS)
    ; remaining entries zero
    times 511 dq 0

; end of boot.S
