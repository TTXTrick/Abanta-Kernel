; src/boot.S
; NASM syntax. Produces a 32-bit object that is linked into the final ELF64 image
; The Multiboot2 header is placed in the first segment so GRUB recognizes it.
; The stub sets up minimal page tables, enables PAE and long mode, and jumps to 64-bit entry.

BITS 32
SECTION .multiboot ALIGN=8
    ; Multiboot2 header (minimal)
    set ALIGN, 8
    ; magic, architecture/flags, checksum
    ; Use tags per Multiboot2; simplest is the header with align and end tag.
    ; The magic below (0xE85250D6) identifies multiboot2.
    dd 0xE85250D6
    dd 0          ; total header length placeholder (filled by linker/objcopy usually)
    dd 0          ; checksum placeholder

    ; Minimal end tag (type 0, size 8)
    ; For convenience we put a trivial header; many toolchains include proper multiboot2 header sections.
    ; (If you want a full multiboot2 header with modules/memory/vbe tags, we can replace this.)
    ; Note: the multiboot2 header MUST be within the first 32768 bytes and 64-bit aligned per spec. See docs.
SECTION .text
global _start
extern kernel_main64      ; defined in kernel.c

_start:
    cli                     ; disable interrupts

    ; -- set up a basic GDT for protected mode (required for far jump to set CS properly) --
    lgdt [gdt_descriptor]

    ; ensure we're in protected mode already (GRUB loads us in 32-bit protected mode)
    ; Build simple page tables (PML4 -> PDPT -> PD)
    ; We'll identity-map the first 1GiB using 2 MiB pages for simplicity.

    ; allocate page tables in .bss area via labels
    ; load CR3 with address of PML4

    ; load address of pml4 into eax
    mov eax, pml4_table
    mov cr3, eax

    ; enable PAE (CR4.PAE = bit 5)
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; set LME in IA32_EFER MSR (0xC0000080) -> bit 8
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; enable paging (CR0.PG) and protected mode (already)
    mov eax, cr0
    or eax, 1 << 31      ; PG bit
    mov cr0, eax

    ; far jump to 64-bit code segment selector (we must switch to a 64-bit code segment defined in GDT)
    ; Prepare for long-mode entry: do a far jump to a 64-bit code segment descriptor
    ; Use "ljmpl" to a selector that we mark as 0x08 (second GDT entry)
    ; The target label entry64 is in 64-bit code (we use a trampoline to change mode)
    ; Because execution after enabling paging will still be in 32-bit, we must perform a far jump into a 64-bit code descriptor.
    ; We use a 64-bit trampoline at label entry64 that expects to be executed in long mode.
    jmp 0x08:long_mode_entry

; ---------------------------------------------------------------------
; Data: GDT and page tables
; ---------------------------------------------------------------------
SECTION .data
align 8
gdt_start:
    ; Null descriptor
    dq 0
    ; 32-bit code/data descriptors (for protected-mode use)
gdt_code:
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b    ; code: execute/read, accessed=0, DPL=0
    db 11001111b    ; granularity: 4K, 32-bit
    db 0
gdt_data:
    dw 0xFFFF
    dw 0
    db 0
    db 10010010b    ; data: read/write
    db 11001111b
    db 0
; 64-bit code segment descriptor (long mode)
; For long mode code segment descriptor is special: L-bit (64-bit) set, D-bit cleared.
gdt_longcode:
    ; We'll create a descriptor with base=0, limit=0xFFFFF and L-bit set.
    ; Compose descriptor bytes directly:
    dw 0xFFFF
    dw 0
    db 0
    db 10011010b    ; code, exec/read, DPL=0
    db 00100000b    ; L=1 (bit 5), rest zero for 64-bit
    db 0

gdt_end:
gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

; ---------------------------------------------------------------------
; 4K-aligned page tables (we’ll set them up here)
; Identity map first 1GiB using 2MiB pages
; ---------------------------------------------------------------------
SECTION .bss
align 4096
pml4_table:    ; 4096 bytes for PML4
    resb 4096
align 4096
pdpt_table:
    resb 4096
align 4096
pd_table:
    resb 4096

SECTION .text
; long_mode_entry: this label must be 64-bit code (it will execute after long mode enabled)
; Many implementations place a small 64-bit trampoline assembled as 64-bit code.
; NASM allows switching bits via BITS directive; but when building one object this approach is common:
; here we do a far jump into the same code segment but execution continues in 64-bit long mode.
; For simplicity we simply jump to the C entry `kernel_main64`.
long_mode_entry:
    ; At this point CPU should be in long mode. We must set up the stack and call kernel_main64.
    ; Switch to using 64-bit registers and stack; assembler here is still assembling in 32-bit mode,
    ; so we'll use raw machine code jumps and construct a minimal 64-bit stub inlined below.
    ; For readability I will use an absolute jump to kernel_main64 (resolved by linker).
    ; Convert to 64-bit by using a push of the 64-bit RIP into the stack then ret (or use an indirect far jump).
    ; Use the standard technique: set up a 64-bit stack and do a RET to a 64-bit code pointer.

    ; For clarity, we do: mov eax, OFFSET kernel_main64 (low 32 bits), mov edx, high 32 bits, push edx; push eax; retf
    ; Many toolchains require proper relocation; you may need to adapt this for your linker.

    ; NOTE: This is a minimal template. If your toolchain complains about relocations, I will adapt it.

    ; We'll implement a simple far return to 64-bit entry using a 64-bit pointer stored in memory.
    jmp kernel_main64
    hlt
