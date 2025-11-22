#include "boot.h"

/* capture_memmap_and_exit:
   - obtains current GetMemoryMap
   - copies it to a buffer allocated via BootServices
   - calls ExitBootServices(ImageHandle, map_key)
   - returns boot_memmap_t with owned buffer (caller must keep it)
*/
EFI_STATUS capture_memmap_and_exit(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, boot_memmap_t *out_map) {
    EFI_STATUS Status;
    UINTN map_size = 0;
    EFI_MEMORY_DESCRIPTOR *map = NULL;
    UINTN map_key = 0;
    UINTN desc_size = 0;
    UINT32 desc_ver = 0;

    /* first call GetMemoryMap to get buffer size */
    Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5,
                               &map_size, map, &map_key, &desc_size, &desc_ver);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        return Status;
    }

    /* allocate a bit more space */
    map_size += 2 * desc_size;
    Status = uefi_call_wrapper(SystemTable->BootServices->AllocatePool, 3,
                               EfiLoaderData, map_size, (void**)&map);
    if (EFI_ERROR(Status)) return Status;

    /* actually get memory map */
    Status = uefi_call_wrapper(SystemTable->BootServices->GetMemoryMap, 5,
                               &map_size, map, &map_key, &desc_size, &desc_ver);
    if (EFI_ERROR(Status)) {
        SystemTable->BootServices->FreePool(map);
        return Status;
    }

    /* attempt ExitBootServices. Must pass the key we just got */
    Status = uefi_call_wrapper(SystemTable->BootServices->ExitBootServices, 2,
                               ImageHandle, map_key);
    if (EFI_ERROR(Status)) {
        /* If it fails, it's often because map changed between calls. Caller can retry (not implemented here). */
        SystemTable->BootServices->FreePool(map);
        return Status;
    }

    /* On success, BootServices are no longer available. Return the map buffer to caller. */
    out_map->map = map;
    out_map->map_size = map_size;
    out_map->map_descriptor_size = desc_size;
    out_map->map_descriptor_version = desc_ver;
    out_map->map_key = map_key;
    return EFI_SUCCESS;
}
