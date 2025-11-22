#include <efi.h>
#include <efilib.h>

EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    InitializeLib(ImageHandle, SystemTable);

    Print(L"Abanta Minimal Kernel Booted!\n");

    CHAR16 Buffer[128];
    UINTN BufferSize;

    while (1) {
        // New prompt
        Print(L"abanta> ");

        BufferSize = sizeof(Buffer);
        Input(L"", Buffer, &BufferSize);

        // Basic “exit” command
        if (StrCmp(Buffer, L"exit") == 0) {
            Print(L"Exiting Abanta shell...\n");
            break;
        }

        // Echo command
        Print(L"You typed: %s\n", Buffer);
    }

    return EFI_SUCCESS;
}
