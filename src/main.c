#include <efi.h>
#include <efilib.h>

EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage;
    EFI_FILE_IO_INTERFACE *FileSystem;
    EFI_FILE_HANDLE Volume;
    EFI_FILE_HANDLE File;
    UINTN FileSize;
    UINT8 *Buffer;

    InitializeLib(ImageHandle, SystemTable);

    Print(L"[Abanta] Bootloader started\n");

    //
    // Get the LoadedImage protocol
    //
    Status = uefi_call_wrapper(
        BS->HandleProtocol,
        3,
        ImageHandle,
        &LoadedImageProtocol,
        (void **)&LoadedImage
    );

    if (EFI_ERROR(Status)) {
        Print(L"Failed to get LoadedImage protocol: %r\n", Status);
        return Status;
    }

    //
    // Get SimpleFileSystem from device handle
    //
    Status = uefi_call_wrapper(
        BS->HandleProtocol,
        3,
        LoadedImage->DeviceHandle,
        &FileSystemProtocol,
        (void **)&FileSystem
    );

    if (EFI_ERROR(Status)) {
        Print(L"Failed to get FileSystem protocol: %r\n", Status);
        return Status;
    }

    //
    // Open root volume
    //
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &Volume);

    if (EFI_ERROR(Status)) {
        Print(L"Failed to open volume: %r\n", Status);
        return Status;
    }

    //
    // Open kernel file
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
    // Get file size
    //
    EFI_FILE_INFO *FileInfo;
    UINTN InfoSize = SIZE_OF_EFI_FILE_INFO + 200;

    Status = uefi_call_wrapper(
        BS->AllocatePool,
        3,
        EfiLoaderData,
        InfoSize,
        (void **)&FileInfo
    );

    Status = uefi_call_wrapper(File->GetInfo, 4,
                               File,
                               &FileInfoGuid,
                               &InfoSize,
                               FileInfo);

    FileSize = FileInfo->FileSize;

    Print(L"kernel.bin size = %d bytes\n", FileSize);

    return EFI_SUCCESS;
}
