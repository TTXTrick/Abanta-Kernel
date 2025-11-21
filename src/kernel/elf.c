#include <stdint.h>
#include <string.h>
#include "elf.h"

/*
    Parse all ELF sections into a helper struct.
    Call this right after loading an ELF file into RAM.
*/
elf_sections_t elf_parse_sections(void *file)
{
    elf_sections_t sec;

    sec.eh = (Elf64_Ehdr*)file;
    sec.shdrs = (Elf64_Shdr*)((uint8_t*)file + sec.eh->e_shoff);
    sec.shnum = sec.eh->e_shnum;
    sec.shstrndx = sec.eh->e_shstrndx;

    Elf64_Shdr *shstr_hdr = &sec.shdrs[sec.shstrndx];
    sec.shstr = (const char*)file + shstr_hdr->sh_offset;

    return sec;
}

/*
    Find a section by name.
    Example: elf_find_section(&secs, ".text")
*/
Elf64_Shdr* elf_find_section(elf_sections_t *sec, const char *name)
{
    for (size_t i = 0; i < sec->shnum; i++) {
        const char *secname = sec->shstr + sec->shdrs[i].sh_name;
        if (strcmp(secname, name) == 0)
            return &sec->shdrs[i];
    }
    return NULL;
}

/*
    Find .symtab (static symbol table)
*/
Elf64_Shdr* elf_find_symtab(elf_sections_t *sec)
{
    return elf_find_section(sec, ".symtab");
}

/*
    Find .strtab (string table used by symbols)
*/
Elf64_Shdr* elf_find_strtab(elf_sections_t *sec)
{
    return elf_find_section(sec, ".strtab");
}

/*
    Find relocation section for a given section.
    Example: for ".text", this finds ".rela.text"
*/
Elf64_Shdr* elf_find_rela(elf_sections_t *sec, const char *for_section)
{
    char buf[64];
    /* Builds ".rela.text" */
    snprintf(buf, sizeof(buf), ".rela%s", for_section);

    return elf_find_section(sec, buf);
}
