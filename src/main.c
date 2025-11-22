#include <Uefi.h>

#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>

#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    //
    // EDK2 does NOT use InitializeLib(). Print() works immediately.
    //
    Print(L"Abanta UEFI Kernel\n");
    Print(L"EDK2 Environment OK.\n\n");

    EFI_STATUS Status;

    //
    // Get LoadedImage protocol
    //
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    Status = SystemTable->BootServices->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID **)&LoadedImage
    );

    if (EFI_ERROR(Status)) {
        Print(L"[ERR] Could not load LoadedImage protocol\n");
        return Status;
    }

    //
    // Get SimpleFileSystem protocol
    //
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FS;
    Status = SystemTable->BootServices->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **)&FS
    );

    if (EFI_ERROR(Status)) {
        Print(L"[ERR] Could not load SimpleFileSystem protocol\n");
        return Status;
    }

    //
    // Open root directory
    //
    EFI_FILE_PROTOCOL *Root;
    Status = FS->OpenVolume(FS, &Root);

    if (EFI_ERROR(Status)) {
        Print(L"[ERR] Could not open volume\n");
        return Status;
    }

    Print(L"[OK] FS mounted — listing files:\n\n");

    //
    // File info buffer
    //
    UINTN BufferSize = sizeof(EFI_FILE_INFO) + 256;
    EFI_FILE_INFO *FileInfo = AllocateZeroPool(BufferSize);

    while (TRUE) {

        BufferSize = sizeof(EFI_FILE_INFO) + 256;
        Status = Root->Read(Root, &BufferSize, FileInfo);

        if (EFI_ERROR(Status) || BufferSize == 0)
            break;

        Print(L"%s\n", FileInfo->FileName);
    }

    FreePool(FileInfo);

    Print(L"\nDone.\n");

    return EFI_SUCCESS;
}
