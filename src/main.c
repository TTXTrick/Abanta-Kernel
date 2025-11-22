#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <string.h>

#include "boot.h"
#include "paging.h"
#include "phys_alloc.h"

/* Keep your original shell helpers (StrCmp, Print, etc). */

/* helper: allocate n pages using BootServices (only valid prior to ExitBootServices) */
static EFI_STATUS allocate_pages_bs(EFI_SYSTEM_TABLE *ST, UINTN pages, EFI_PHYSICAL_ADDRESS *addr) {
    return uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pages, addr);
}

/* helper: build very small page tables in preallocated pages (PML4 at pml4_phys) */
static void build_pagetables_concrete(EFI_PHYSICAL_ADDRESS pml4_phys, uint64_t identity_size) {
    /* We'll build:
       PML4 -> PDPT -> PD -> 2MiB page entries to identity-map low physical memory.
       We assume the memory for PML4/PDPT/PD is contiguous and already zeroed.
       Layout:
         page 0: PML4
         page 1: PDPT
         pages 2..N: PDs (enough to map identity_size with 2MiB pages)
    */
    uint64_t *pml4 = (uint64_t*)(uintptr_t)pml4_phys;
    uint64_t *pdpt = (uint64_t*)(uintptr_t)(pml4_phys + 4096);
    uint64_t *pd = (uint64_t*)(uintptr_t)(pml4_phys + 8192);

    /* zero them */
    for (int i=0;i<512;i++){ pml4[i]=0; pdpt[i]=0; }
    for (int i=0;i<512;i++){ pd[i]=0; }

    /* connect PML4[0] -> PDPT (present + RW) */
    pml4[0] = ((uint64_t)(pml4_phys + 4096)) | 0x3; /* present + rw */

    /* connect PDPT[0] -> PD (present + RW) */
    pdpt[0] = ((uint64_t)(pml4_phys + 8192)) | 0x3;

    /* fill PD with 2MiB entries */
    uint64_t pages_2mb = (identity_size + (2ULL<<20) - 1) / (2ULL<<20);
    uint64_t addr = 0;
    for (uint64_t i=0; i < pages_2mb && i < 512; ++i) {
        /* create 2MiB page entry: physical addr | flags | PS bit (bit 7) */
        pd[i] = (addr & 0x000ffffffffff000ULL) | (0x83); /* present + rw + user (optional) + PS */
        addr += (2ULL << 20);
    }
}

/* simple wrapper that uses write_cr3 inline assembly */
static inline void load_cr3(uint64_t pml4_phys) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(pml4_phys) : "memory");
}

EFI_STATUS EFIAPI efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Abanta kernel starting (preparing to become real kernel)\n");

    /* STEP A: Reserve pages for page tables and a small page allocator BEFORE ExitBootServices */
    const UINTN page_table_pages = 16; /* 16 pages (64KiB) for PML4/PDPT/PD tables */
    EFI_PHYSICAL_ADDRESS page_table_phys = 0;
    Status = allocate_pages_bs(SystemTable, page_table_pages, &page_table_phys);
    if (EFI_ERROR(Status)) { Print(L"AllocatePages for page tables failed: %r\n", Status); return Status; }
    /* Zero the allocated area */
    memset((void*)(uintptr_t)page_table_phys, 0, page_table_pages * 4096);

    /* Reserve a chunk for the kernel physical allocator: e.g., 64 pages = 256 KiB */
    const UINTN phys_alloc_pages = 64;
    EFI_PHYSICAL_ADDRESS phys_alloc_base = 0;
    Status = allocate_pages_bs(SystemTable, phys_alloc_pages, &phys_alloc_base);
    if (EFI_ERROR(Status)) { Print(L"AllocatePages for phys allocator failed: %r\n", Status); return Status; }
    memset((void*)(uintptr_t)phys_alloc_base, 0, phys_alloc_pages * 4096);

    Print(L"Reserved page-tables at %p, phys-allocator at %p\n", (void*)(uintptr_t)page_table_phys, (void*)(uintptr_t)phys_alloc_base);

    /* STEP B: capture memmap and ExitBootServices */
    boot_memmap_t memmap;
    Status = capture_memmap_and_exit(ImageHandle, SystemTable, &memmap);
    if (EFI_ERROR(Status)) {
        Print(L"ExitBootServices failed: %r\n", Status);
        return Status;
    }
    Print(L"ExitBootServices succeeded — we are now on our own\n");

    /* STEP C: build identity mapping in reserved page-table pages and load CR3 */
    /* We'll identity-map the first 1 GiB (enough for many kernels + devices) */
    const uint64_t identity_size = (1ULL << 30); /* 1 GiB */
    /* Build page tables using the pre-allocated page_table_phys */
    build_pagetables_concrete(page_table_phys, identity_size);
    /* Now set CR3 to our PML4 physical address */
    load_cr3((uint64_t)page_table_phys);
    Print(L"Loaded page tables (CR3=%p)\n", (void*)(uintptr_t)page_table_phys);

    /* STEP D: initialize tiny physical allocator using previously reserved region */
    phys_init((uintptr_t)phys_alloc_base, phys_alloc_pages * 4096);
    Print(L"phys allocator initialized (base=%p, size=%u KiB)\n", (void*)(uintptr_t)phys_alloc_base, (unsigned)(phys_alloc_pages*4));

    /* Now we have: ExitBootServices called, our page tables active, and kernel allocator usable.
       We can allocate pages, run code, etc., all under our control.
    */

    /* Kernel shell loop (same as before), but using kernel allocator and with prompt abanta> */
    static CHAR16 line[256];
    UINTN line_len = 0;
    EFI_INPUT_KEY key;

    Print(L"Abanta kernel ready. Type `help`.\n");

    for (;;) {
        Print(L"abanta> ");
        line_len = 0;
        line[0] = L'\0';

        while (1) {
            Status = SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
            if (Status == EFI_NOT_READY) { /* spin */ __asm__ volatile("pause"); continue; }
            if (EFI_ERROR(Status)) { Print(L"\nReadKey error: %r\n", Status); return Status; }

            if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
                Print(L"\n");
                line[line_len] = L'\0';
                break;
            }
            if (key.UnicodeChar == CHAR_BACKSPACE) {
                if (line_len > 0) { line_len--; line[line_len] = L'\0'; Print(L"\x08 \x08"); }
                continue;
            }
            if (key.UnicodeChar == 0) continue;
            if (line_len + 1 < sizeof(line)/sizeof(line[0])) {
                line[line_len++] = key.UnicodeChar;
                line[line_len] = L'\0';
                Print(L"%c", key.UnicodeChar);
            }
        }

        if (StrCmp(line, L"help") == 0) {
            Print(L"Commands: help, alloc, free, mmap, halt\n");
            continue;
        }
        if (StrCmp(line, L"alloc") == 0) {
            void *p = kmalloc(4096);
            if (!p) Print(L"alloc failed\n"); else Print(L"kmalloc -> %p\n", p);
            continue;
        }
        if (StrCmp(line, L"halt") == 0) {
            Print(L"Halted\n");
            for (;;) { __asm__ volatile("hlt"); }
        }
        if (StrCmp(line, L"mmap") == 0) {
            /* print the UEFI memory map we captured earlier */
            UINTN desc_size = memmap.map_descriptor_size;
            UINTN entries = memmap.map_size / desc_size;
            EFI_MEMORY_DESCRIPTOR *d = memmap.map;
            Print(L"UEFI Memory Map (entries: %u)\n", entries);
            for (UINTN i = 0; i < entries; ++i) {
                Print(L"  Type %u, Phys:0x%lx, Pages:0x%lx\n", d->Type, d->PhysicalStart, d->NumberOfPages);
                d = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)d + desc_size);
            }
            continue;
        }

        Print(L"Unknown: %s\n", line);
    }

    return EFI_SUCCESS;
}
