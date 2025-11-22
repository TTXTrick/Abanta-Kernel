/*
  Abanta UEFI "kernel-like" app with a tiny shell prompt "abanta>"

  - Builds as a UEFI application using GNU-EFI headers/libraries.
  - Provides a simple line-editing prompt, a few built-in commands,
    and a facility to load ELF modules from the same FAT partition.
  - NOTE: this runs under UEFI (uses Boot Services). It is not a full
    bare-metal kernel. If you want ExitBootServices + true kernel mode,
    I can add a proper loader and kernel bootstrap later.
*/

#include <efi.h>
#include <efilib.h>
#include <protocol/loaded_image.h>
#include <protocol/simple_file_system.h>
#include <guid/file_info.h>
#include <stdint.h>
#include <string.h>

#define SHELL_BUF_SIZE 512

static void print_prompt(void) {
    Print(L"abanta> ");
}

static void handle_command(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST, CHAR16 *buf);

/* simple line-oriented shell using ReadKeyStroke */
EFI_STATUS
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    InitializeLib(ImageHandle, SystemTable);

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    Print(L"Abanta (UEFI kernel-like) — type 'help'\n\n");

    static CHAR16 line[SHELL_BUF_SIZE];
    UINTN line_len = 0;
    EFI_INPUT_KEY key;

    print_prompt();

    for (;;) {
        Status = uefi_call_wrapper(SystemTable->ConIn->ReadKeyStroke, 2, SystemTable->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            /* polite tiny delay to avoid busy-hogging the CPU */
            uefi_call_wrapper(SystemTable->BootServices->Stall, 1, 500);
            continue;
        }
        if (EFI_ERROR(Status)) {
            Print(L"\nReadKeyStroke error: %r\n", Status);
            return Status;
        }

        /* handle control keys */
        if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            line[line_len] = L'\0';
            Print(L"\n");
            handle_command(ImageHandle, SystemTable, line);
            line_len = 0;
            line[0] = L'\0';
            print_prompt();
            continue;
        }
        if (key.UnicodeChar == CHAR_BACKSPACE) {
            if (line_len > 0) {
                line_len--;
                line[line_len] = L'\0';
                /* move cursor back, overwrite, move back */
                Print(L"\x08 \x08");
            } else {
                /* beep */
                Print(L"\a");
            }
            continue;
        }
        if (key.UnicodeChar == 0) continue; /* ignore function keys etc. */

        if (line_len + 1 < SHELL_BUF_SIZE) {
            line[line_len++] = key.UnicodeChar;
            line[line_len] = L'\0';
            Print(L"%c", key.UnicodeChar);
        } else {
            Print(L"\a"); /* buffer full */
        }
    }

    return EFI_SUCCESS;
}

/* --- helpers --- */

/* read an entire file from the same device the EFI image was loaded from.
   Path is an EFI wide string like L"kernel.bin" or L"modules/mod.elf"
   Caller must FreePool the buffer when done. */
static EFI_STATUS read_entire_file_from_image(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST, CHAR16 *Path, void **out_buf, UINTN *out_size) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFs = NULL;
    EFI_FILE_PROTOCOL *Root = NULL;
    EFI_FILE_PROTOCOL *File = NULL;
    EFI_FILE_INFO *FileInfo = NULL;
    UINTN FileInfoSize = 0;
    void *file_buffer = NULL;
    UINTN file_size = 0;

    /* get LoadedImage protocol for our image */
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;

    /* get SimpleFileSystem from the device handle */
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle, &SimpleFileSystemProtocol, (void**)&SimpleFs);
    if (EFI_ERROR(Status)) return Status;

    Status = uefi_call_wrapper(SimpleFs->OpenVolume, 2, SimpleFs, &Root);
    if (EFI_ERROR(Status)) return Status;

    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, Path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) return Status;

    FileInfoSize = 0;
    Status = uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &FileInfoSize, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        File->Close(File);
        return EFI_UNSUPPORTED;
    }
    Status = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, FileInfoSize, (void**)&FileInfo);
    if (EFI_ERROR(Status)) { File->Close(File); return Status; }
    Status = uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    if (EFI_ERROR(Status)) { File->Close(File); ST->BootServices->FreePool(FileInfo); return Status; }

    file_size = (UINTN)FileInfo->FileSize;
    ST->BootServices->FreePool(FileInfo);

    Status = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, file_size, &file_buffer);
    if (EFI_ERROR(Status)) { File->Close(File); return Status; }
    UINTN read = file_size;
    Status = uefi_call_wrapper(File->Read, 3, File, &read, file_buffer);
    if (EFI_ERROR(Status) || read != file_size) {
        File->Close(File);
        ST->BootServices->FreePool(file_buffer);
        return EFI_DEVICE_ERROR;
    }
    File->Close(File);

    *out_buf = file_buffer;
    *out_size = file_size;
    return EFI_SUCCESS;
}

/* minimal built-ins: help, clear, echo, mem, loadmod (reads file), run (reads file and reports),
   reboot, halt, dlsym (not implemented fully but placeholder) */
static void print_help(void) {
    Print(L"Commands:\n");
    Print(L"  help           - show help\n");
    Print(L"  clear          - clear screen\n");
    Print(L"  echo <text>    - echo text\n");
    Print(L"  mem            - show memory map entries\n");
    Print(L"  loadmod <path> - load module file into memory (keeps buffer)\n");
    Print(L"  run <path>     - read ELF file and report (no exec in this build)\n");
    Print(L"  reboot         - reboot\n");
    Print(L"  halt           - halt machine\n");
}

/* handle a single command line */
static void handle_command(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST, CHAR16 *buf) {
    if (!buf || StrLen(buf) == 0) return;

    if (StrCmp(buf, L"help") == 0) { print_help(); return; }
    if (StrCmp(buf, L"clear") == 0) { ST->ConOut->ClearScreen(ST->ConOut); return; }
    if (StrnCmp(buf, L"echo ", 5) == 0) { Print(L"%s\n", buf + 5); return; }
    if (StrCmp(buf, L"reboot") == 0) { ST->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL); return; }
    if (StrCmp(buf, L"halt") == 0) { Print(L"Halted (spin)\n"); for (;;) { __asm__ volatile("hlt"); } }

    if (StrCmp(buf, L"mem") == 0) {
        UINTN mem_map_size = 0;
        EFI_MEMORY_DESCRIPTOR *mem_map = NULL;
        UINTN map_key;
        UINTN desc_size;
        UINT32 desc_version;
        EFI_STATUS st = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
        if (st == EFI_BUFFER_TOO_SMALL) {
            mem_map_size += 2 * desc_size;
            st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, mem_map_size, (void**)&mem_map);
            if (!EFI_ERROR(st)) {
                st = uefi_call_wrapper(BS->GetMemoryMap, 5, &mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
                if (!EFI_ERROR(st)) {
                    UINTN entry_count = mem_map_size / desc_size;
                    Print(L"Memory map entries: %u\n", entry_count);
                    EFI_MEMORY_DESCRIPTOR *desc = mem_map;
                    for (UINTN i = 0; i < entry_count; ++i) {
                        Print(L"  Type %u, Phys:0x%lx, Pages:0x%lx\n",
                              desc->Type, desc->PhysicalStart, desc->NumberOfPages);
                        desc = (EFI_MEMORY_DESCRIPTOR*)((UINT8*)desc + desc_size);
                    }
                } else {
                    Print(L"GetMemoryMap failed (2): %r\n", st);
                }
                ST->BootServices->FreePool(mem_map);
            } else {
                Print(L"AllocatePool failed: %r\n", st);
            }
        } else {
            Print(L"GetMemoryMap failed: %r\n", st);
        }
        return;
    }

    if (StrnCmp(buf, L"run ", 4) == 0) {
        CHAR16 *path = buf + 4;
        void *filebuf = NULL;
        UINTN filesize = 0;
        EFI_STATUS st = read_entire_file_from_image(ImageHandle, ST, path, &filebuf, &filesize);
        if (EFI_ERROR(st)) {
            Print(L"run: read failed: %r\n", st);
            return;
        }
        Print(L"Read %s (size %lu bytes). No exec in this build.\n", path, filesize);
        ST->BootServices->FreePool(filebuf);
        return;
    }

    if (StrnCmp(buf, L"loadmod ", 8) == 0) {
        CHAR16 *path = buf + 8;
        void *filebuf = NULL;
        UINTN filesize = 0;
        EFI_STATUS st = read_entire_file_from_image(ImageHandle, ST, path, &filebuf, &filesize);
        if (EFI_ERROR(st)) {
            Print(L"loadmod: read failed: %r\n", st);
            return;
        }
        /* keep it in memory for now (memory leak by design in this tiny demo) */
        Print(L"Module %s loaded (size %lu bytes)\n", path, filesize);
        return;
    }

    if (StrnCmp(buf, L"dlsym ", 6) == 0) {
        CHAR16 *name16 = buf + 6;
        char name[256]; int j=0;
        for (UINTN i=0; name16[i] && j < (int)sizeof(name)-1; ++i) name[j++] = (char)name16[i];
        name[j]=0;
        Print(L"dlsym: lookup '%a' (not implemented)\n", name);
        return;
    }

    Print(L"Unknown command: %s\n", buf);
}
