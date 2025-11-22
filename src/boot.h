#ifndef BOOT_H
#define BOOT_H

#include <efi.h>
#include <efilib.h>

typedef struct {
    EFI_MEMORY_DESCRIPTOR *map;
    UINTN map_size;
    UINTN map_descriptor_size;
    UINT32 map_descriptor_version;
    UINTN map_key;
} boot_memmap_t;

/* capture memory map and call ExitBootServices */
EFI_STATUS capture_memmap_and_exit(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, boot_memmap_t *out_map);

#endif
