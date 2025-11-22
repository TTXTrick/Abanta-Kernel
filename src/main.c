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
    // Initialize UEFI library
    InitializeLib(ImageHandle, SystemTable);

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    Print(L"Abanta UEFI Kernel\n");
    Print(L"Building environment OK.\n\n");

    EFI_STATUS Status;

    //
    // Get LOADED_IMAGE_PROTOCOL from ImageHandle
    //
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;
    Status = SystemTable->BootServices->HandleProtocol(
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (VOID **)&LoadedImage
    );

    if (EFI_ERROR(Status)) {
        Print(L"[ERR] Unable to get LoadedImage protocol\n");
        return Status;
    }

    //
    // Get SimpleFileSystem protocol from the device the kernel was loaded from
    //
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FS;
    Status = SystemTable->BootServices->HandleProtocol(
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (VOID **)&FS
    );

    if (EFI_ERROR(Status)) {
        Print(L"[ERR] Unable to get SimpleFileSystem protocol\n");
        return Status;
    }

    //
    // Open the root directory
    //
    EFI_FILE_PROTOCOL *Root;
    Status = FS->OpenVolume(FS, &Root);

    if (EFI_ERROR(Status)) {
        Print(L"[ERR] Unable to open filesystem volume\n");
        return Status;
    }

    Print(L"[OK] Volume opened.\n");
    Print(L"Listing files:\n\n");

    //
    // Allocate a buffer for file info
    //
    UINTN BufferSize = sizeof(EFI_FILE_INFO) + 512;
    EFI_FILE_INFO *FileInfo = AllocateZeroPool(BufferSize);

    while (TRUE) {

        BufferSize = sizeof(EFI_FILE_INFO) + 512;
        Status = Root->Read(Root, &BufferSize, FileInfo);

        if (EFI_ERROR(Status) || BufferSize == 0)
            break;

        Print(L"%s\n", FileInfo->FileName);
    }

    FreePool(FileInfo);

    Print(L"\nAbanta kernel finished.\n");

    return EFI_SUCCESS;
}
