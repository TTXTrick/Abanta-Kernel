#include <efi.h>
#include <efilib.h>

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Abanta Kernel Loaded via GNU-EFI!\n");
    Print(L"Hello from UEFI environment.\n");

    // Wait for key press
    Print(L"Press any key to exit...\n");
    WaitForSingleEvent(SystemTable->ConIn->WaitForKey, 0);

    return EFI_SUCCESS;
}
