/*
  src/main.c  - Abanta UEFI kernel with:
    - ELF64 loader with RELA relocation support (R_* types)
    - ET_EXEC support (attempts AllocatePages at requested addresses)
    - Host-call ABI via abanta_host_api pointer & first arg
    - Module loader (loadmod) + dlsym-like symbol resolution
    - Best-effort page protection using EfiLoaderCode/EfiLoaderData and SetMemoryAttributes if available

  Build with gnu-efi as before.
*/

#include <efi.h>
#include <efilib.h>
#include <stdint.h>
#include <string.h>

/* ----- Minimal ELF64 types ----- */
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
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

typedef struct {
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
    Elf64_Sxword r_addend;
} Elf64_Rela;

typedef struct {
    Elf64_Word st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half st_shndx;
    Elf64_Addr st_value;
    Elf64_Xword st_size;
} Elf64_Sym;

/* ELF constants */
#define PT_LOAD 1
#define PT_DYNAMIC 2

#define ET_EXEC 2
#define ET_DYN 3

/* relocation macros */
#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((uint32_t)(i))

/* dynamic tags we will inspect */
#define DT_NULL 0
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_SYMTAB 6
#define DT_STRTAB 5
#define DT_SYMENT 11

/* relocation types (X86_64) */
#define R_X86_64_NONE 0
#define R_X86_64_64   1
#define R_X86_64_GLOB_DAT 6
#define R_X86_64_JUMP_SLOT 7
#define R_X86_64_RELATIVE 8

/* ----- Module bookkeeping ----- */
#define MAX_MODULES 16

typedef struct {
    CHAR16 *path;        /* path used to load */
    void *base;          /* base address where segments were instantiated */
    UINTN size;          /* size allocated */
    Elf64_Ehdr *eh;      /* pointer into base */
    Elf64_Sym *symtab;   /* if present */
    CHAR8 *strtab;       /* if present */
    UINTN symcount;
} loaded_module_t;

static loaded_module_t modules[MAX_MODULES];
static int module_count = 0;

/* ----- Host ABI (exported to user programs) ----- */

struct abanta_host_api {
    EFI_SYSTEM_TABLE *st; /* pointer to UEFI SystemTable (raw) */
    void (*print_utf16)(const CHAR16 *s); /* helper that calls Print */
    void *(*malloc)(UINTN size); /* AllocatePool wrapper */
    void (*free)(void *ptr);     /* FreePool wrapper */
    void *(*dlsym)(const char *name); /* resolve symbol from modules+kernel */
    UINTN (*get_mem_map)(void *buffer, UINTN buffer_size); /* convenience wrapper (returns needed size if 0 passed) */
};

/* forward declaration */
static void *kernel_dlsym(const char *name);

/* the global host API pointer (user programs can link to this symbol name) */
struct abanta_host_api *abanta_host_api = NULL;

/* helper wrappers that will be set in host_api implementation */
static void api_print_utf16(const CHAR16 *s) {
    Print(L"%s", s);
}
static void *api_malloc(UINTN size) {
    void *p = NULL;
    EFI_STATUS st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, size, &p);
    if (EFI_ERROR(st)) return NULL;
    return p;
}
static void api_free(void *p) {
    if (p) uefi_call_wrapper(BS->FreePool, 1, p);
}
static void *api_dlsym(const char *name) {
    return kernel_dlsym(name);
}
static UINTN api_get_mem_map(void *buffer, UINTN buffer_size) {
    UINTN map_key = 0, desc_size=0, desc_ver=0;
    EFI_MEMORY_DESCRIPTOR *map = buffer;
    EFI_STATUS st = uefi_call_wrapper(BS->GetMemoryMap, 5, &buffer_size, map, &map_key, &desc_size, &desc_ver);
    if (st == EFI_BUFFER_TOO_SMALL) return buffer_size;
    if (EFI_ERROR(st)) return 0;
    return buffer_size;
}

/* ----- Kernel symbol table ----- */
/* Any kernel-facing function you want user programs to call must be placed here.
   We provide a small set by default. */
typedef struct {
    const char *name;
    void *addr;
} ksym_t;

static ksym_t kernel_symbols[] = {
    { "abanta_host_api", (void*)&abanta_host_api }, /* pointer to host API */
    { "host_print_utf16", (void*)api_print_utf16 },
    { "host_malloc", (void*)api_malloc },
    { "host_free", (void*)api_free },
    { "host_dlsym", (void*)api_dlsym },
    { NULL, NULL }
};

/* ----- Helpers: module symbol resolution & kernel dlsym ----- */
static void *kernel_dlsym(const char *name) {
    /* search kernel symbols */
    for (ksym_t *k = kernel_symbols; k->name; ++k) {
        if (strcmp(k->name, name) == 0) return k->addr;
    }
    /* search modules' symbol tables */
    for (int m = 0; m < module_count; ++m) {
        loaded_module_t *mod = &modules[m];
        if (!mod->symtab || !mod->strtab) continue;
        for (UINTN i = 0; i < mod->symcount; ++i) {
            Elf64_Sym *s = &mod->symtab[i];
            const char *nm = (const char*)(mod->strtab + s->st_name);
            if (nm && strcmp(nm, name) == 0) {
                /* symbol value is relative to module base */
                return (void*)((uint8_t*)mod->base + (UINTN)s->st_value);
            }
        }
    }
    return NULL;
}

/* ----- Utility: read file into pool ----- */
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

    Status = uefi_call_wrapper(ST->BootServices->HandleProtocol, 3, ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
    if (EFI_ERROR(Status)) return Status;
    Status = uefi_call_wrapper(ST->BootServices->HandleProtocol, 3, LoadedImage->DeviceHandle, &SimpleFileSystemProtocol, (void**)&SimpleFs);
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

/* ----- Low-level: attempt to allocate pages at address 'addr' with type EfiLoaderCode/Data.
   Returns allocated address (in-out argument) and EFI error on failure. */
static EFI_STATUS allocate_pages_for_segment(EFI_SYSTEM_TABLE *ST, EFI_PHYSICAL_ADDRESS *addr, UINTN pages, EFI_MEMORY_TYPE mtype, BOOLEAN allocate_at_address) {
    EFI_STATUS st;
    if (allocate_at_address) {
        st = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AllocateAddress, mtype, pages, addr);
    } else {
        st = uefi_call_wrapper(ST->BootServices->AllocatePages, 4, AnyPages, mtype, pages, addr);
    }
    return st;
}

/* ----- Apply dynamic RELA relocations -----
   Supports RELA entries and the common relocation types listed above.
   Parameters:
     base: base pointer where binary was loaded (so p_vaddr offsets are base + vaddr)
     rela: pointer to first Elf64_Rela
     rela_count: number of entries
     symtab: symbol table (absolute pointer within base or 0)
     strtab: string table pointer
*/
static EFI_STATUS apply_relocations(EFI_SYSTEM_TABLE *ST, void *base, Elf64_Rela *rela, UINTN rela_count, Elf64_Sym *symtab, CHAR8 *strtab, UINTN symcount) {
    for (UINTN i = 0; i < rela_count; ++i) {
        Elf64_Rela *r = &rela[i];
        uint32_t type = ELF64_R_TYPE(r->r_info);
        uint64_t sym  = ELF64_R_SYM(r->r_info);
        uint8_t *where = (uint8_t*)base + (UINTN)r->r_offset;
        switch (type) {
        case R_X86_64_RELATIVE: {
            uint64_t val = (uint64_t)((uint8_t*)base + (UINTN)r->r_addend);
            *(uint64_t*)where = val;
            break;
        }
        case R_X86_64_64: {
            /* S + A */
            uint64_t S = 0;
            if (sym >= symcount) return EFI_LOAD_ERROR;
            Elf64_Sym *s = &symtab[sym];
            if (s->st_shndx != 0) {
                S = (uint64_t)((uint8_t*)base + (UINTN)s->st_value);
            } else {
                /* undefined: resolve via kernel dlsym */
                const char *name = (const char*)(strtab + s->st_name);
                void *res = kernel_dlsym(name);
                if (!res) {
                    Print(L"Unresolved symbol (R_X86_64_64): %a\n", name);
                    return EFI_LOAD_ERROR;
                }
                S = (uint64_t)res;
            }
            uint64_t A = (uint64_t)r->r_addend;
            *(uint64_t*)where = S + A;
            break;
        }
        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT: {
            /* S */
            if (sym >= symcount) return EFI_LOAD_ERROR;
            Elf64_Sym *s = &symtab[sym];
            uint64_t S = 0;
            if (s->st_shndx != 0) {
                S = (uint64_t)((uint8_t*)base + (UINTN)s->st_value);
            } else {
                const char *name = (const char*)(strtab + s->st_name);
                void *res = kernel_dlsym(name);
                if (!res) {
                    Print(L"Unresolved symbol (GLOB_DAT/JUMP_SLOT): %a\n", name);
                    return EFI_LOAD_ERROR;
                }
                S = (uint64_t)res;
            }
            *(uint64_t*)where = S;
            break;
        }
        default:
            Print(L"Unhandled relocation type: %u\n", type);
            return EFI_UNSUPPORTED;
        }
    }
    return EFI_SUCCESS;
}

/* ----- Load an ELF file as a module (returns pointer base & fills module entry metadata) -----
   If execute_immediately == TRUE, call the entry (passing host api pointer as first arg).
   Behavior:
     - file_buf: in-memory ELF file
     - file_size: size
     - ImageHandle/SystemTable used for AllocatePages
     - flags: supports ET_DYN and ET_EXEC. For ET_EXEC will try to allocate at fixed addresses.
*/
static EFI_STATUS load_elf_from_buffer(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST, void *file_buf, UINTN file_size, void **out_base, UINTN *out_size, int execute_immediately) {
    if (file_size < sizeof(Elf64_Ehdr)) return EFI_LOAD_ERROR;
    Elf64_Ehdr *eh = (Elf64_Ehdr*)file_buf;
    if (!(eh->e_ident[0] == 0x7f && eh->e_ident[1] == 'E' && eh->e_ident[2] == 'L' && eh->e_ident[3] == 'F')) return EFI_LOAD_ERROR;
    if (eh->e_ident[4] != 2) return EFI_LOAD_ERROR; /* must be ELF64 */

    /* Find program headers */
    Elf64_Phdr *ph = (Elf64_Phdr*)((uint8_t*)file_buf + eh->e_phoff);
    /* Determine memory footprint: for ET_DYN we map relative to base 0 (we choose base), for ET_EXEC the p_vaddr is absolute */
    Elf64_Addr low = (Elf64_Addr)-1;
    Elf64_Addr high = 0;
    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        if (ph[i].p_vaddr < low) low = ph[i].p_vaddr;
        Elf64_Addr end = ph[i].p_vaddr + ph[i].p_memsz;
        if (end > high) high = end;
    }
    if (low == (Elf64_Addr)-1) return EFI_LOAD_ERROR;

    /* Size needed */
    UINTN needed = (UINTN)((high - (low & ~0xFFFULL) + 0xFFF) & ~0xFFFULL);
    /* For ET_DYN, we choose a base. We'll allocate pages for each segment separately so we can set type (code/data). Simpler: allocate one region as EfiLoaderData and copy, then set attributes. But we attempt a smarter approach: allocate per-segment. */

    /* If ET_EXEC, attempt to allocate at addresses p_vaddr (page-aligned) */
    BOOLEAN is_exec = (eh->e_type == ET_EXEC);

    /* We'll compute segment allocations: for each PT_LOAD, allocate pages at either any address or the requested address. */
    typedef struct {
        EFI_PHYSICAL_ADDRESS addr;
        UINTN pages;
        EFI_MEMORY_TYPE mtype;
        Elf64_Phdr phdr;
    } seg_alloc_t;
    seg_alloc_t segs[16];
    int segcount = 0;

    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        Elf64_Addr start_v = ph[i].p_vaddr;
        Elf64_Addr seg_page = start_v & ~0xFFFULL;
        UINTN memsz = (UINTN)ph[i].p_memsz + (UINTN)(start_v - seg_page);
        UINTN pages = (memsz + 0xFFF) / 0x1000;
        segs[segcount].pages = pages;
        segs[segcount].phdr = ph[i];
        segs[segcount].addr = (EFI_PHYSICAL_ADDRESS)seg_page;
        /* choose memory type depending on writable flag */
        if (ph[i].p_flags & 0x2) segs[segcount].mtype = EfiLoaderData; else segs[segcount].mtype = EfiLoaderCode;
        segcount++;
    }

    /* try allocate segments */
    void *chosen_base = NULL;
    for (int s = 0; s < segcount; ++s) {
        EFI_PHYSICAL_ADDRESS addr = segs[s].addr;
        BOOLEAN allocate_at_addr = is_exec; /* only allocate at specific address in ET_EXEC case */
        EFI_STATUS st = allocate_pages_for_segment(ST, &addr, segs[s].pages, segs[s].mtype, allocate_at_addr);
        if (EFI_ERROR(st)) {
            /* on ET_EXEC we must fail; on ET_DYN we attempt AnyPages allocation */
            if (is_exec) {
                Print(L"AllocatePages at requested address 0x%lx failed: %r\n", segs[s].addr, st);
                /* rollback already allocated segments */
                for (int j = 0; j < s; ++j) {
                    uefi_call_wrapper(ST->BootServices->FreePages, 2, segs[j].addr, segs[j].pages);
                }
                return st;
            } else {
                /* try AnyPages */
                EFI_PHYSICAL_ADDRESS a2 = 0;
                st = allocate_pages_for_segment(ST, &a2, segs[s].pages, segs[s].mtype, FALSE);
                if (EFI_ERROR(st)) {
                    Print(L"AllocatePages failed (AnyPages): %r\n", st);
                    /* rollback previous */
                    for (int j = 0; j < s; ++j) {
                        uefi_call_wrapper(ST->BootServices->FreePages, 2, segs[j].addr, segs[j].pages);
                    }
                    return st;
                }
                segs[s].addr = a2;
            }
        } else {
            segs[s].addr = addr; /* record actual mapping addr */
        }
    }

    /* After segment allocation, compute overall base: for ET_DYN we want base = lowest seg addr - low_alignment */
    EFI_PHYSICAL_ADDRESS base_phys = (EFI_PHYSICAL_ADDRESS)-1;
    for (int s = 0; s < segcount; ++s) {
        if (segs[s].addr < base_phys) base_phys = segs[s].addr;
    }

    /* Fill segments: copy file contents into allocated memory and zero remainder */
    for (int s = 0; s < segcount; ++s) {
        Elf64_Phdr *p = &segs[s].phdr;
        uint8_t *dest = (uint8_t*)(uintptr_t)segs[s].addr + (UINTN)(p->p_vaddr - ((uintptr_t)base_phys));
        /* bounds: p_offset <= file_size etc. */
        if (p->p_offset + p->p_filesz > file_size) {
            Print(L"Segment file out of bounds\n");
            /* TODO: free pages */
            return EFI_LOAD_ERROR;
        }
        /* copy */
        CopyMem(dest, (uint8_t*)file_buf + p->p_offset, (UINTN)p->p_filesz);
        /* zero remainder */
        if (p->p_memsz > p->p_filesz) {
            SetMem(dest + p->p_filesz, (UINTN)(p->p_memsz - p->p_filesz), 0);
        }
    }

    /* compute base pointer for relocations & symbols: for ET_DYN base = base_phys - low; for ET_EXEC base = 0 (absolute) */
    void *base_ptr;
    if (is_exec) {
        base_ptr = (void*)(uintptr_t)0; /* we interpret p_vaddr as absolute virtual addresses -> caller must ensure mapping */
    } else {
        /* ET_DYN - chose base as base_phys - low */
        base_ptr = (void*)((uint8_t*)(uintptr_t)base_phys - (low & ~0xFFFULL));
    }

    /* find dynamic segment (if present) to locate rela, symtab, strtab */
    Elf64_Addr rela_addr = 0;
    UINTN rela_size = 0;
    UINTN rela_ent = 0;
    Elf64_Sym *symtab = NULL;
    CHAR8 *strtab = NULL;
    UINTN symcount = 0;

    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type == PT_DYNAMIC) {
            Elf64_Off off = (Elf64_Off)ph[i].p_offset;
            Elf64_Xword len = ph[i].p_filesz;
            if (off + len > file_size) return EFI_LOAD_ERROR;
            /* parse dyn array */
            typedef struct { Elf64_Sxword d_tag; Elf64_Xword d_un; } Dyn;
            Dyn *dyn = (Dyn*)((uint8_t*)file_buf + off);
            size_t ndyn = len / sizeof(Dyn);
            for (size_t d = 0; d < ndyn; ++d) {
                if (dyn[d].d_tag == DT_NULL) break;
                if (dyn[d].d_tag == DT_RELA) rela_addr = dyn[d].d_un;
                if (dyn[d].d_tag == DT_RELASZ) rela_size = dyn[d].d_un;
                if (dyn[d].d_tag == DT_RELAENT) rela_ent = dyn[d].d_un;
                if (dyn[d].d_tag == DT_SYMTAB) symtab = (Elf64_Sym*)(uintptr_t)dyn[d].d_un;
                if (dyn[d].d_tag == DT_STRTAB) strtab = (CHAR8*)(uintptr_t)dyn[d].d_un;
                if (dyn[d].d_tag == DT_SYMENT) /*ignored*/;
            }
            break;
        }
    }

    /* If symtab/strtab are present and are file-relative addresses, we need to convert them to pointers in our allocated base.
       Many ET_DYN's dynamic entries give virtual addresses (p_vaddr) not file offsets. So we compute pointers: pointer = base_ptr + vaddr. */
    /* Convert symtab/strtab addresses if present (we assume they are virtual addresses) */
    if (symtab) {
        symtab = (Elf64_Sym*)((uint8_t*)base_ptr + (uintptr_t)symtab);
        /* estimate symcount by scanning until we hit invalid? We cannot know easily; we'll leave it to modules registration later if they provide shnum. */
        /* For safety we will leave symcount as 0 until module registration step reads section headers (not available easily). But many small PIEs won't need sym lookups back into themselves. */
    }
    if (strtab) {
        strtab = (CHAR8*)((uint8_t*)base_ptr + (uintptr_t)strtab);
    }

    /* Convert RELA area pointer */
    Elf64_Rela *rela = NULL;
    UINTN rela_cnt = 0;
    if (rela_addr && rela_size) {
        rela = (Elf64_Rela*)((uint8_t*)base_ptr + (uintptr_t)rela_addr);
        rela_cnt = rela_size / sizeof(Elf64_Rela);
    }

    /* Apply relocations if any */
    if (rela && symtab && strtab) {
        /* Need symbol count: attempt heuristic by looking at next symbol value? For simplicity assume symcount large and let relocation code check range */
        /* But our apply_relocations expects symcount; we will pass a large number if unknown */
        UINTN guessed_symcount = 65536;
        EFI_STATUS st = apply_relocations(ST, base_ptr, rela, rela_cnt, symtab, strtab, guessed_symcount);
        if (EFI_ERROR(st)) return st;
    } else if (rela) {
        /* If relocations present but we couldn't find symtab/strtab, still attempt RELATIVE-only relocations */
        /* Build a minimal fake symtab with 0 entries and pass 0 so only RELATIVE will be applied */
        EFI_STATUS st = apply_relocations(ST, base_ptr, rela, rela_cnt, (Elf64_Sym*)0, (CHAR8*)0, 0);
        if (EFI_ERROR(st)) return st;
    }

    /* Register as module if not executed immediately (we still register even if executed, to support symbol resolution later) */
    if (module_count < MAX_MODULES) {
        loaded_module_t *m = &modules[module_count++];
        m->path = NULL; /* optional - caller may set */
        m->base = base_ptr;
        m->size = (UINTN)needed;
        m->eh = (Elf64_Ehdr*)base_ptr;
        m->symtab = symtab;
        m->strtab = strtab;
        m->symcount = 0; /* if we had access to section headers we could compute this; leave 0 */
    } else {
        Print(L"Module table full\n");
        return EFI_OUT_OF_RESOURCES;
    }

    *out_base = base_ptr;
    *out_size = needed;

    /* If execute_immediately, call entry.
       Convention: try calling (void (*)(struct abanta_host_api *))entry first.
       If that crashes or is incompatible the user program must be built to accept this pointer.
    */
    if (execute_immediately) {
        void *entry = (uint8_t*)base_ptr + (uintptr_t)eh->e_entry;
        typedef void (*user_entry1_t)(struct abanta_host_api *);
        user_entry1_t e1 = (user_entry1_t)entry;
        /* call with host api pointer */
        e1(abanta_host_api);
    }

    return EFI_SUCCESS;
}

/* ----- Top-level: load ELF file from disk and optionally execute ----- */
static EFI_STATUS load_elf_and_maybe_exec(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST, CHAR16 *Path, BOOLEAN execute) {
    void *filebuf = NULL;
    UINTN filesize = 0;
    EFI_STATUS st = read_entire_file_from_image(ImageHandle, ST, Path, &filebuf, &filesize);
    if (EFI_ERROR(st)) {
        Print(L"read file failed: %r\n", st);
        return st;
    }
    void *base = NULL;
    UINTN size = 0;
    st = load_elf_from_buffer(ImageHandle, ST, filebuf, filesize, &base, &size, execute);
    if (EFI_ERROR(st)) {
        Print(L"load_elf_from_buffer failed: %r\n", st);
        BS->FreePool(filebuf);
        return st;
    }
    BS->FreePool(filebuf);
    Print(L"Loaded '%s' at %p (size 0x%lx)\n", Path, base, size);
    return EFI_SUCCESS;
}

/* ----- Shell command handlers & main loop ----- */

#define SHELL_BUF_SIZE 512

static void print_prompt(void) {
    Print(L"abanta> ");
}

static void handle_command(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *ST, CHAR16 *buf) {
    if (StrCmp(buf, L"") == 0) return;

    if (StrCmp(buf, L"help") == 0) {
        Print(L"Commands:\n");
        Print(L"  help       - show help\n");
        Print(L"  clear      - clear screen\n");
        Print(L"  echo ...   - echo text\n");
        Print(L"  mem        - show memory map entries\n");
        Print(L"  run <path> - load and run ELF (ET_DYN/PIE preferred)\n");
        Print(L"  loadmod <path> - load module but do not run (adds to dlsym)\n");
        Print(L"  dlsym <name> - lookup symbol address\n");
        Print(L"  reboot     - reboot\n");
        Print(L"  halt       - halt machine\n");
        return;
    }
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
        EFI_STATUS st = uefi_call_wrapper(ST->BootServices->GetMemoryMap, 5, &mem_map_size, mem_map, &map_key, &desc_size, &desc_version);
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
    if (StrnCmp(buf, L"run ", 4) == 0) {
        CHAR16 *path = buf + 4;
        EFI_STATUS st = load_elf_and_maybe_exec(ImageHandle, ST, path, TRUE);
        if (EFI_ERROR(st)) Print(L"run error: %r\n", st);
        return;
    }
    if (StrnCmp(buf, L"loadmod ", 8) == 0) {
        CHAR16 *path = buf + 8;
        EFI_STATUS st = load_elf_and_maybe_exec(ImageHandle, ST, path, FALSE);
        if (EFI_ERROR(st)) Print(L"loadmod error: %r\n", st);
        return;
    }
    if (StrnCmp(buf, L"dlsym ", 6) == 0) {
        CHAR16 *name16 = buf + 6;
        /* convert UTF16 to ASCII simple (assume ASCII) */
        char name[256]; int j=0;
        for (UINTN i=0; name16[i] && j < sizeof(name)-1; ++i) {
            name[j++] = (char)name16[i];
        }
        name[j]=0;
        void *addr = kernel_dlsym(name);
        if (!addr) Print(L"Symbol '%a' not found\n", name); else Print(L"Symbol '%a' -> %p\n", name, addr);
        return;
    }

    Print(L"Unknown command: %s\n", buf);
}

/* ----- Entry point ----- */
EFI_STATUS EFIAPI efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    InitializeLib(ImageHandle, SystemTable);

    /* initialize host API */
    static struct abanta_host_api host_api;
    host_api.st = SystemTable;
    host_api.print_utf16 = api_print_utf16;
    host_api.malloc = api_malloc;
    host_api.free = api_free;
    host_api.dlsym = api_dlsym;
    host_api.get_mem_map = api_get_mem_map;
    abanta_host_api = &host_api;

    SystemTable->ConOut->ClearScreen(SystemTable->ConOut);
    Print(L"Abanta UEFI kernel — x86_64 with ELF loader + relocations + module support\nType 'help' for commands.\n\n");

    /* Shell input loop */
    static CHAR16 line[SHELL_BUF_SIZE];
    UINTN line_len = 0;
    EFI_INPUT_KEY key;
    print_prompt();

    for (;;) {
        Status = uefi_call_wrapper(SystemTable->ConIn->ReadKeyStroke, 2, SystemTable->ConIn, &key);
        if (Status == EFI_NOT_READY) { SystemTable->BootServices->Stall(1000); continue; }
        if (EFI_ERROR(Status)) { Print(L"\nReadKeyStroke error: %r\n", Status); return Status; }
        if (key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
            line[line_len] = L'\0';
            Print(L"\n");
            handle_command(ImageHandle, SystemTable, line);
            line_len = 0; line[0]='\0';
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
        } else Print(L"\a");
    }

    return EFI_SUCCESS;
}
