typedef struct {
    Elf64_Ehdr *eh;
    Elf64_Shdr *shdrs;
    const char *shstr;
    size_t shnum;
    size_t shstrndx;
} elf_sections_t;
