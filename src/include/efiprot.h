#ifndef EFIPROT_H
#define EFIPROT_H

#include <stdint.h>
#include "efidef.h"

// SIMPLE TEXT INPUT
typedef struct {
    EFI_STATUS (*Reset)(void *Self, BOOLEAN ExtendedVerification);
    EFI_STATUS (*ReadKeyStroke)(void *Self, void *Key);
    void *WaitForKey;
} EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

// BOOT SERVICES
typedef struct {
    EFI_TABLE_HEADER Hdr;

    void *RaiseTPL;
    void *RestoreTPL;

    EFI_STATUS (*AllocatePages)(int Type, int MemoryType, UINT64 Pages, UINT64 *Memory);
    EFI_STATUS (*FreePages)(UINT64, UINT64);

    EFI_STATUS (*GetMemoryMap)(UINT64 *MemoryMapSize, void *MemoryMap,
                               UINT64 *MapKey, UINT64 *DescriptorSize,
                               UINT32 *DescriptorVersion);

    EFI_STATUS (*AllocatePool)(int PoolType, UINT64 Size, void **Buffer);
    EFI_STATUS (*FreePool)(void *Buffer);

    EFI_STATUS (*CreateEvent)(uint32_t Type, UINT64 NotifyTpl,
                              void *NotifyFunction, void *NotifyContext, void **Event);

    EFI_STATUS (*SetTimer)(void *Event, uint64_t Type, uint64_t TriggerTime);

    EFI_STATUS (*WaitForEvent)(UINT64 NumberOfEvents, void **Event, UINT64 *Index);

    EFI_STATUS (*SignalEvent)(void *Event);
    EFI_STATUS (*CloseEvent)(void *Event);

    void *InstallProtocolInterface;
    void *UninstallProtocolInterface;

    EFI_STATUS (*HandleProtocol)(EFI_HANDLE Handle, void *Protocol, void **Interface);

    void *RegisterProtocolNotify;

    void *LocateHandle;
    void *LocateDevicePath;

    void *InstallConfigurationTable;

    void (*LoadImage)(void);
    void (*StartImage)(void);

    void *Exit;
    void *UnloadImage;
    void *ExitBootServices;

    // many more to add later if needed…
} EFI_BOOT_SERVICES;

// RUNTIME SERVICES (not used much yet)
typedef struct {
    EFI_TABLE_HEADER Hdr;
    // leave empty now
} EFI_RUNTIME_SERVICES;

typedef struct {
    EFI_GUID VendorGuid;
    void *VendorTable;
} EFI_CONFIGURATION_TABLE;

#endif
