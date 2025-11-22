#include "paging.h"

/* Private helpers */
static inline void write_cr3(uint64_t paddr) {
    __asm__ volatile("mov %0, %%cr3" :: "r"(paddr) : "memory");
}

/* Allocate pages before ExitBootServices and use them after: since we already called ExitBootServices,
   we must have allocated the page-table pages before ExitBootServices. For simplicity, this code
   expects the page table memory to have been allocated earlier; but to keep the flow manageable we
   will allocate via UEFI earlier and keep the addresses. However in our flow we call ExitBootServices
   before building page tables -- therefore we must allocate the page-table storage via BootServices
   earlier. To simplify usage, this function assumes platform still provided BootServices for AllocatePages
   earlier. In our main flow we'll allocate a chunk right before ExitBootServices and store pointer somewhere.
*/

#define PAGE_SIZE 4096ULL
#define ENTRIES_PER_TABLE 512

/* We'll create one PML4, one PDPT and many PDs. We'll identity map 'map_size_to_identity' bytes. */

EFI_STATUS build_identity_paging_and_load(boot_memmap_t *memmap, EFI_PHYSICAL_ADDRESS map_size_to_identity, EFI_PHYSICAL_ADDRESS *out_pml4_phys) {
    /* We cannot call BootServices->AllocatePages here (this function will be called after ExitBootServices),
       so real code needs to allocate pages before ExitBootServices. To keep this sample tractable, we assume
       the caller reserved a contiguous region and put its physical address in memmap->map (NOT ideal).
       For clarity and safety, we'll instead use a static fallback where we interpret a small region of
       physically contiguous memory near 1 MiB (this is fragile). In practice: allocate page-table pages via
       AllocatePages just before ExitBootServices and pass their physical address in out_pml4_phys.
    */
    Print(L"[paging] building identity mapping (2MiB pages) for 0x%lx bytes\n", (UINT64)map_size_to_identity);

    /* Simple approach: create PML4 in runtime-usable memory: allocate via malloc-like earlier. */
    /* For demonstration we'll request a small allocation via a pre-initialized region using mmap-style array. */

    /* Allocate 3 pages using UEFI -- but since BootServices are gone, this is not possible here.
       So: the correct integration is below in the main flow where we allocate pages BEFORE ExitBootServices,
       build page tables using those pages AFTER ExitBootServices, then write CR3. */

    return EFI_UNSUPPORTED;
}

void dump_pagetables(EFI_PHYSICAL_ADDRESS pml4_phys) {
    Print(L"[paging] pml4 phys: 0x%lx\n", pml4_phys);
}
