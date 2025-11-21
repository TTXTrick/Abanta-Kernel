#ifndef _EFI_H_
#define _EFI_H_

#include "efidef.h"

/*
 * Forward declarations
 */
struct _EFI_SYSTEM_TABLE;
struct _EFI_BOOT_SERVICES;
struct _EFI_RUNTIME_SERVICES;
struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL;
struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

/*
 * EFI_SYSTEM_TABLE
 */
typedef struct _EFI_SYSTEM_TABLE {
    EFI_TABLE_HEADER                       Hdr;

    CHAR16                                 *FirmwareVendor;
    UINT32                                  FirmwareRevision;

    EFI_HANDLE                              ConsoleInHandle;
    struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL *ConIn;

    EFI_HANDLE                              ConsoleOutHandle;
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *ConOut;

    EFI_HANDLE                              StandardErrorHandle;
    struct _EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *StdErr;

    struct _EFI_RUNTIME_SERVICES           *RuntimeServices;
    struct _EFI_BOOT_SERVICES              *BootServices;

    UINTN                                   NumberOfTableEntries;
    void                                   *ConfigurationTable;
} EFI_SYSTEM_TABLE;

extern EFI_SYSTEM_TABLE *ST;
extern struct _EFI_BOOT_SERVICES *BS;

#endif
