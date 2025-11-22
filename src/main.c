#include <efi.h>
#include <efilib.h>

EFI_STATUS
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Abanta kernel stub loaded!\n");

    return EFI_SUCCESS;
}
