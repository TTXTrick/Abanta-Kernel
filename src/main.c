/* src/main.c
   UEFI kernel with ELF64 (PIE/ET_DYN) loader and a tiny shell.
   Build with gnu-efi like before.
*/

#include <efi.h>
#include <efilib.h>
#include <stdint.h>

#define SHELL_BUF_SIZE 512

/* Minimal ELF64 types (avoid depending on system headers). */
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint64_t Elf64_Offt;
typedef uint64_t Elf64_Offx;
typedef uint64_t Elf64_Size;

#define EI_NIDENT 16

typedef struct {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half e_type;
    Elf64_Half e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;
    Elf64_Off  e_phoff;
    Elf64_Off  e_shoff;
    Elf64_Word e_flags;
    Elf64_Half e_ehsize;
    Elf64_Half e_phentsize;
    Elf64_Half e_phnum;
    Elf64_Half e_shentsize;
    Elf64_Half e_shnum;
    Elf64_Half e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    Elf64_Word p_type;
    Elf64_Word p_flags;
    Elf64_Off  p_offset;
    Elf64_Addr p_vaddr;
    Elf64_Addr p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
} Elf64_Phdr;

#define PT_LOAD 1

/* Forward */
EFI_STATUS EFIAPI efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

static void print_prompt(void) {
    Print(L"abanta> ");
}

/* --- ELF loader -------------------------------------------------------
   load_elf_from_file:
     - ImageHandle: used to locate filesystem
     - SystemTable: boot services
     - Path: UTF-16 file name relative to root (e.g. L"EFI\\BOOT\\user.elf")
     - out_entry: receives pointer to entry (callable as void (*)(void))
   Returns EFI_SUCCESS on success, else error.
   Only supports ELF64 ET_DYN (position independent) or ET_EXEC if built sensibly.
   It maps all PT_LOAD segments into one big allocated buffer at 'base'.
   --------------------------------------------------------------------- */
static EFI_STATUS load_elf_from_file(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST, CHAR16 *Path, void **out_entry) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE *LoadedImage = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFs = NULL;
    EFI_FILE_PROTOCOL *Root = NULL;
    EFI_FILE_PROTOCOL *File = NULL;
    EFI_FILE_INFO *FileInfo = NULL;
    UINTN FileInfoSize = 0;
    void *file_buffer = NULL;
    UINTN file_size = 0;

    /* Get LoadedImage for device handle */
    Status = uefi_call_wrapper(ST->BootServices->HandleProtocol, 3, ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;

    /* Get SimpleFS on the device that loaded our image */
    Status = uefi_call_wrapper(ST->BootServices->HandleProtocol, 3, LoadedImage->DeviceHandle, &SimpleFileSystemProtocol, (void**)&SimpleFs);
    if (EFI_ERROR(Status)) return Status;

    /* Open root volume */
    Status = uefi_call_wrapper(SimpleFs->OpenVolume, 2, SimpleFs, &Root);
    if (EFI_ERROR(Status)) return Status;

    /* Open the file */
    Status = uefi_call_wrapper(Root->Open, 5, Root, &File, Path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    /* Get file size via GetInfo */
    FileInfoSize = 0;
    Status = uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &FileInfoSize, NULL);
    if (Status != EFI_BUFFER_TOO_SMALL) {
        /* Unexpected; try to read chunkwise instead */
        /* fallback: read progressively (not implemented for brevity) */
        File->Close(File);
        return EFI_UNSUPPORTED;
    }

    /* allocate buffer for EFI_FILE_INFO */
    Status = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, FileInfoSize, (void**)&FileInfo);
    if (EFI_ERROR(Status)) { File->Close(File); return Status; }

    Status = uefi_call_wrapper(File->GetInfo, 4, File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
    if (EFI_ERROR(Status)) { File->Close(File); ST->BootServices->FreePool(FileInfo); return Status; }

    file_size = (UINTN)FileInfo->FileSize;
    ST->BootServices->FreePool(FileInfo);

    /* allocate a buffer and read entire file */
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

    /* parse ELF header */
    if (file_size < sizeof(Elf64_Ehdr)) {
        ST->BootServices->FreePool(file_buffer);
        return EFI_LOAD_ERROR;
    }

    Elf64_Ehdr *eh = (Elf64_Ehdr*)file_buffer;
    /* check magic */
    if (!(eh->e_ident[0] == 0x7f && eh->e_ident[1] == 'E' && eh->e_ident[2] == 'L' && eh->e_ident[3] == 'F')) {
        ST->BootServices->FreePool(file_buffer);
        return EFI_LOAD_ERROR;
    }
    if (eh->e_ident[4] != 2) { /* EI_CLASS == ELFCLASS64 (2) */
        ST->BootServices->FreePool(file_buffer);
        return EFI_LOAD_ERROR;
    }

    /* compute memory footprint: maximum p_vaddr + memsz among PT_LOAD */
    Elf64_Phdr *ph = (Elf64_Phdr*)((uint8_t*)file_buffer + eh->e_phoff);
    if (eh->e_phnum == 0) {
        ST->BootServices->FreePool(file_buffer);
        return EFI_LOAD_ERROR;
    }

    Elf64_Addr highest_end = 0;
    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        Elf64_Addr end = ph[i].p_vaddr + ph[i].p_memsz;
        if (end > highest_end) highest_end = end;
    }
    if (highest_end == 0) {
        ST->BootServices->FreePool(file_buffer);
        return EFI_LOAD_ERROR;
    }

    /* allocate a contiguous buffer 'base' of size = highest_end rounded up */
    UINTN alloc_size = (UINTN)((highest_end + 0xFFF) & ~0xFFFULL);
    void *base = NULL;
    Status = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, alloc_size, &base);
    if (EFI_ERROR(Status)) { ST->BootServices->FreePool(file_buffer); return Status; }

    /* zero entire region (so .bss is zero) */
    SetMem(base, alloc_size, 0);

    /* copy PT_LOAD segments: file offset -> base + p_vaddr */
    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        /* bounds checks */
        if (ph[i].p_offset + ph[i].p_filesz > (Elf64_Xword)file_size) {
            ST->BootServices->FreePool(file_buffer);
            ST->BootServices->FreePool(base);
            return EFI_LOAD_ERROR;
        }
        void *dest = (uint8_t*)base + (UINTN)ph[i].p_vaddr;
        void *src  = (uint8_t*)file_buffer + (UINTN)ph[i].p_offset;
        CopyMem(dest, src, (UINTN)ph[i].p_filesz);
        /* remaining bytes up to memsz are already zeroed */
    }

    /* compute the runtime entry pointer */
    void *entry = (void*)((uint8_t*)base + (UINTN)eh->e_entry);

    /* free file buffer but keep base allocated so program is in memory */
    ST->BootServices->FreePool(file_buffer);

    /* deliver entry */
    *out_entry = entry;
    return EFI_SUCCESS;
}

/* sample command handler (extend previous shell) */
static void handle_command(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST, CHAR16 *buf) {
    if (StrCmp(buf, L"") == 0) return;

    if (StrCmp(buf, L"help") == 0) {
        Print(L"Commands:\n  help  clear  echo ...  mem  reboot  halt  run <path>\n");
        return;
    }
    if (StrCmp(buf, L"clear") == 0) {
        ST->ConOut->ClearScreen(ST->ConOut);
        return;
    }
    if (StrnCmp(buf, L"echo ", 5) == 0) {
        Print(L"%s\n", buf + 5);
        return;
    }
    if (StrCmp(buf, L"reboot") == 0) {
        ST->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
        return;
    }
    if (StrCmp(buf, L"halt") == 0) {
        Print(L"Halted (spin). Use reset in QEMU.\n");
        for (;;) { __asm__ volatile("hlt"); }
    }
    if (StrCmp(buf, L"mem") == 0) {
        UINTN mem_map_size = 0;
        EFI_MEMORY_DESCRIPTOR *mem_map = NULL;
        UINTN map_key;
        UINTN desc_size;
        UINT32 desc_version;
        EFI_STATUS st;
        st = uefi_call_wrapper(ST->BootServices->GetMemoryMap, 5, &mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
        if (st == EFI_BUFFER_TOO_SMALL) {
            mem_map_size += 2 * desc_size;
            st = uefi_call_wrapper(ST->BootServices->AllocatePool, 3, EfiLoaderData, mem_map_size, (void**)&mem_map);
            if (!EFI_ERROR(st)) {
                st = uefi_call_wrapper(ST->BootServices->GetMemoryMap, 5, &mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
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

    /* new: run <path> */
    if (StrnCmp(buf, L"run ", 4) == 0) {
        CHAR16 *path = buf + 4;
        void *entry = NULL;
        EFI_STATUS st = load_elf_from_file(ImageHandle, ST, path, &entry);
        if (EFI_ERROR(st)) {
            Print(L"Failed to load '%s': %r\n", path, st);
            return;
        }
        Print(L"Loaded '%s', entry at %p — executing...\n", path, entry);
        /* Call the entry as a simple void function with no args */
        typedef void (*user_entry_t)(void);
        user_entry_t fn = (user_entry_t)entry;
        fn();
        Print(L"\nUser program returned to kernel.\n");
        return;
    }

    /* Unknown */
    Print(L"Unknown command: %s\n", buf);
}

EFI_STATUS EFIAPI efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    InitializeLib(ImageHandle, SystemTable);

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    Print(L"Abanta UEFI kernel — x86_64 with ELF loader\nType 'help' for commands.\n\n");

    static CHAR16 line[SHELL_BUF_SIZE];
    UINTN line_len = 0;
    EFI_INPUT_KEY key;
    print_prompt();

    for (;;) {
        Status = uefi_call_wrapper(SystemTable->ConIn->ReadKeyStroke, 2, SystemTable->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            SystemTable->BootServices->Stall(1000); /* 1ms */
            continue;
        } else if (EFI_ERROR(Status)) {
            Print(L"\nReadKeyStroke error: %r\n", Status);
            return Status;
        }

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
                Print(L"\x08 \x08");
            }
            continue;
        }

        if (key.UnicodeChar == 0) continue;

        if (line_len + 1 < SHELL_BUF_SIZE) {
            line[line_len++] = key.UnicodeChar;
            line[line_len] = L'\0';
            Print(L"%c", key.UnicodeChar);
        } else {
            Print(L"\a");
        }
    }

    return EFI_SUCCESS;
}
