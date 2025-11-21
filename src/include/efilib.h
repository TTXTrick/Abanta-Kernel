#ifndef EFILIB_H
#define EFILIB_H

#include <efi.h>
#include <efidef.h>
#include <efiprot.h>
#include <eficon.h>

// Basic helper functions used by many tutorials
static inline void Print(EFI_SYSTEM_TABLE *SystemTable, CHAR16 *str) {
    SystemTable->ConOut->OutputString(SystemTable->ConOut, str);
}

#endif
