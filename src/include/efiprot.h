#ifndef _EFI_PROT_H_
#define _EFI_PROT_H_

#include "efidef.h"

/*
 * EFI Input Key structure
 */
typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

/*
 * Forward declarations for simple text input
 */
typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

typedef EFI_STATUS (*EFI_INPUT_RESET)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    BOOLEAN ExtendedVerification
);

typedef EFI_STATUS (*EFI_INPUT_READ_KEY)(
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    EFI_INPUT_KEY *Key
);

/*
 * Simple Text Input Protocol
 */
struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_INPUT_RESET      Reset;
    EFI_INPUT_READ_KEY   ReadKeyStroke;
    EFI_EVENT            WaitForKey;
};

/*
 * Boot Services function pointer typedefs
 */

typedef EFI_STATUS (*EFI_ALLOCATE_PAGES)(
    UINT32 Type,
    EFI_MEMORY_TYPE MemoryType,
    UINTN Pages,
    EFI_PHYSICAL_ADDRESS *Memory
);

typedef EFI_STATUS (*EFI_FREE_PAGES)(
    EFI_PHYSICAL_ADDRESS Memory,
    UINTN Pages
);

typedef EFI_STATUS (*EFI_GET_MEMORY_MAP)(
    UINTN *MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN *MapKey,
    UINTN *DescriptorSize,
    UINT32 *DescriptorVersion
);

typedef EFI_STATUS (*EFI_EXIT_BOOT_SERVICES)(
    EFI_HANDLE ImageHandle,
    UINTN MapKey
);

/*
 * EFI_BOOT_SERVICES structure
 */
typedef struct _EFI_BOOT_SERVICES {
    EFI_TABLE_HEADER                Hdr;

    // Task Priority Services (unused by most loaders)
    void *RaiseTPL;
    void *RestoreTPL;

    // Memory Services
    EFI_ALLOCATE_PAGES              AllocatePages;
    EFI_FREE_PAGES                  FreePages;
    void *GetMemoryMap; // pointer patched below
    void *AllocatePool;
    void *FreePool;

    // Event, Timer, Protocol, Image Services (we only define a few)
    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;

    // Misc
    void *InstallProtocolInterface;
    void *UninstallProtocolInterface;
    void *HandleProtocol;
    void *RegisterProtocolNotify;

    // More Boot Services...
    void *LoadImage;
    void *StartImage;
    void *Exit;
    void *UnloadImage;

    EFI_EXIT_BOOT_SERVICES ExitBootServices;

} EFI_BOOT_SERVICES;

#endif
