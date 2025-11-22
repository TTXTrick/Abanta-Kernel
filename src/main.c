#include <efi.h>
#include <efilib.h>

EFI_STATUS
EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
    InitializeLib(ImageHandle, SystemTable);

    Print(L"[Abanta] Bootloader started\n");

    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_FILE_IO_INTERFACE *FileSystem;
    EFI_FILE_HANDLE Volume;
    EFI_FILE_HANDLE File;
    EFI_FILE_INFO *FileInfo;

    //
    // Get LoadedImage protocol
    //
    Status = uefi_call_wrapper(
        BS->HandleProtocol,
        3,
        ImageHandle,
        &gEfiLoadedImageProtocolGuid,
        (void **)&LoadedImage
    );

    if (EFI_ERROR(Status)) {
        Print(L"Failed to get LoadedImage protocol: %r\n", Status);
        return Status;
    }

    //
    // Get Simple FileSystem protocol
    //
    Status = uefi_call_wrapper(
        BS->HandleProtocol,
        3,
        LoadedImage->DeviceHandle,
        &gEfiSimpleFileSystemProtocolGuid,
        (void **)&FileSystem
    );

    if (EFI_ERROR(Status)) {
        Print(L"Failed to get FileSystem protocol: %r\n", Status);
        return Status;
    }

    //
    // Open root directory
    //
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Volume);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to open volume: %r\n", Status);
        return Status;
    }

    //
    // Open kernel.bin
    //
    Status = uefi_call_wrapper(
        Volume->Open,
        5,
        Volume,
        &File,
        L"kernel.bin",
        EFI_FILE_MODE_READ,
        0
    );

    if (EFI_ERROR(Status)) {
        Print(L"kernel.bin not found: %r\n", Status);
        return Status;
    }

    //
    // Read file info to get file size
    //
    UINTN InfoSize = SIZE_OF_EFI_FILE_INFO + 200;

    Status = uefi_call_wrapper(BS->AllocatePool,
                               3,
                               EfiLoaderData,
                               InfoSize,
                               (void **)&FileInfo);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to allocate pool: %r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(File->GetInfo,
                               4,
                               File,
                               &gEfiFileInfoGuid,
                               &InfoSize,
                               FileInfo);

    if (EFI_ERROR(Status)) {
        Print(L"GetInfo failed: %r\n", Status);
        return Status;
    }

    Print(L"kernel.bin size = %lu bytes\n", FileInfo->FileSize);

    return EFI_SUCCESS;
}
