/* src/main.c
   Build as a UEFI application using gnu-efi.
   Provides keyboard input and a minimal command shell. */

#include <efi.h>
#include <efilib.h>

#define SHELL_BUF_SIZE 256

/* forward */
EFI_STATUS EFIAPI efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

static void print_prompt(void) {
    Print(L"abanta> ");
}

static void handle_command(EFI_SYSTEM_TABLE *ST, CHAR16 *buf) {
    if (StrCmp(buf, L"") == 0) return;

    if (StrCmp(buf, L"help") == 0) {
        Print(L"Commands:\n");
        Print(L"  help    - show this help\n");
        Print(L"  clear   - clear screen\n");
        Print(L"  echo ...- echo text\n");
        Print(L"  mem     - show simple memory info\n");
        Print(L"  reboot  - reset the machine\n");
        Print(L"  halt    - halt (spin)\n");
        return;
    }

    if (StrCmp(buf, L"clear") == 0) {
        ST->ConOut->ClearScreen(ST->ConOut);
        return;
    }

    if (StrnCmp(buf, L"echo ", 5) == 0) {
        CHAR16 *msg = buf + 5;
        Print(L"%s\n", msg);
        return;
    }

    if (StrCmp(buf, L"reboot") == 0) {
        /* EfiResetCold will reboot */
        ST->RuntimeServices->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
        return;
    }

    if (StrCmp(buf, L"halt") == 0) {
        Print(L"Halted. Press any key to continue (or Ctrl+C in QEMU).\n");
        /* busy-wait with HLT (no guaranteed effect under UEFI, but acceptable for hobby kernel) */
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    if (StrCmp(buf, L"mem") == 0) {
        /* Simple memory info via GetMemoryMap (not comprehensive) */
        UINTN mem_map_size = 0;
        EFI_MEMORY_DESCRIPTOR *mem_map = NULL;
        UINTN map_key;
        UINTN desc_size;
        UINT32 desc_version;
        EFI_STATUS st;

        /* call once to get size */
        st = ST->BootServices->GetMemoryMap(&mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
        if (st == EFI_BUFFER_TOO_SMALL) {
            /* allocate buffer */
            mem_map_size += 2 * desc_size;
            st = ST->BootServices->AllocatePool(EfiLoaderData, mem_map_size, (void**)&mem_map);
            if (!EFI_ERROR(st)) {
                st = ST->BootServices->GetMemoryMap(&mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
                if (!EFI_ERROR(st)) {
                    UINTN entry_count = mem_map_size / desc_size;
                    Print(L"Memory map entries: %u\n", entry_count);
                    EFI_MEMORY_DESCRIPTOR *desc = mem_map;
                    for (UINTN i = 0; i < entry_count; ++i) {
                        Print(L"  Type %u, Phys:%lx, Pages:%lx\n",
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

    /* unknown command */
    Print(L"Unknown command: %s\n", buf);
}

EFI_STATUS EFIAPI efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    InitializeLib(ImageHandle, SystemTable);

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    Print(L"Abanta UEFI kernel — x86_64\nType 'help' for commands.\n\n");

    /* Shell input buffer (UTF-16 CHAR16). We'll implement simple line editing. */
    static CHAR16 line[SHELL_BUF_SIZE];
    UINTN line_len = 0;

    /* Ensure input is in raw mode? UEFI text input doesn't require special mode; we poll ReadKeyStroke. */
    EFI_INPUT_KEY key;

    print_prompt();

    for (;;) {
        Status = SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &key);
        if (Status == EFI_NOT_READY) {
            /* no key available; yield CPU to avoid busy-looping */
            /* Use Stall to sleep for a short time to be polite to host CPU */
            SystemTable->BootServices->Stall(1000); /* 1 ms */
            continue;
        } else if (EFI_ERROR(Status)) {
            /* unexpected error */
            Print(L"\nReadKeyStroke error: %r\n", Status);
            return Status;
        }

        /* handle key */
        if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            /* Enter: terminate line and execute */
            line[line_len] = L'\0';
            Print(L"\n");
            handle_command(SystemTable, line);
            /* reset buffer */
            line_len = 0;
            line[0] = L'\0';
            print_prompt();
            continue;
        }

        if (key.UnicodeChar == CHAR_BACKSPACE) {
            if (line_len > 0) {
                line_len--;
                line[line_len] = L'\0';
                /* move cursor back, print space, move back */
                Print(L"\x08 \x08"); /* backspace, space, backspace */
            }
            continue;
        }

        /* ignore other control chars (including function keys) if UnicodeChar == 0 */
        if (key.UnicodeChar == 0) {
            continue;
        }

        /* printable char: append if space remains */
        if (line_len + 1 < SHELL_BUF_SIZE) {
            line[line_len++] = key.UnicodeChar;
            line[line_len] = L'\0';
            /* echo char */
            Print(L"%c", key.UnicodeChar);
        } else {
            /* buffer full, beep or ignore */
            Print(L"\a");
        }
    }

    /* never reached, but required signature */
    return EFI_SUCCESS;
}
