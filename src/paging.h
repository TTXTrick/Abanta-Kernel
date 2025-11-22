#ifndef PAGING_H
#define PAGING_H

#include <efi.h>
#include <efilib.h>
#include "boot.h"

/* Build simple identity mapping for the low physical address space (e.g. first N bytes).
   This uses 2 MiB pages where possible (large pages). Returns physical address of new PML4. */
EFI_STATUS build_identity_paging_and_load(boot_memmap_t *memmap, EFI_PHYSICAL_ADDRESS map_size_to_identity, EFI_PHYSICAL_ADDRESS *out_pml4_phys);

/* helper to free created page tables if desired (not strictly necessary) */
void dump_pagetables(EFI_PHYSICAL_ADDRESS pml4_phys);

#endif
