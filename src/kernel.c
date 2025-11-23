/* kernel.c - x86_64 multiboot2 kernel (Abanta)
   - minimal VGA text console
   - simple shell with history
   - reads Multiboot2 tags passed by GRUB (if present) for memory map and modules
   - supports 'help', 'clear', 'echo', 'mem', 'modules', 'run <idx>'
*/

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* VGA text */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
static volatile uint16_t *VGA = (volatile uint16_t*)0xB8000;
static size_t vga_row = 0, vga_col = 0;

static inline uint16_t vga_entry(unsigned char ch, unsigned char color) {
    return (uint16_t)ch | (uint16_t)color << 8;
}

static void vga_clear(void) {
    uint16_t blank = vga_entry(' ', 0x07);
    for (size_t r = 0; r < VGA_HEIGHT; ++r)
        for (size_t c = 0; c < VGA_WIDTH; ++c)
            VGA[r * VGA_WIDTH + c] = blank;
    vga_row = vga_col = 0;
}

static void vga_putch(char ch) {
    if (ch == '\n') {
        vga_col = 0;
        if (++vga_row >= VGA_HEIGHT) vga_row = 0;
        return;
    }
    VGA[vga_row * VGA_WIDTH + vga_col] = vga_entry((unsigned char)ch, 0x07);
    if (++vga_col >= VGA_WIDTH) { vga_col = 0; if (++vga_row >= VGA_HEIGHT) vga_row = 0; }
}

static void vga_puts(const char *s) {
    while (*s) vga_putch(*s++);
}

/* tiny printf (only %s, %d, %x, %p) */
static void print_hex(uint64_t x) {
    char buf[17]; int i = 0;
    if (x == 0) { vga_puts("0"); return; }
    while (x) {
        int d = x & 0xF;
        buf[i++] = (d < 10) ? ('0'+d) : ('a' + (d-10));
        x >>= 4;
    }
    while (i--) vga_putch(buf[i]);
}

static void print_dec(uint64_t v) {
    char buf[32]; int i = 0;
    if (v == 0) { vga_putch('0'); return; }
    while (v) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i--) vga_putch(buf[i]);
}

static void kprintf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    for (const char *p = fmt; *p; ++p) {
        if (*p != '%') { vga_putch(*p); continue; }
        ++p;
        if (*p == 's') { char *s = va_arg(ap, char*); vga_puts(s ? s : "(null)"); }
        else if (*p == 'd') { print_dec(va_arg(ap, uint64_t)); }
        else if (*p == 'x') { print_hex(va_arg(ap, uint64_t)); }
        else if (*p == 'p') { vga_puts("0x"); print_hex((uint64_t)va_arg(ap, void*)); }
        else { vga_putch(*p); }
    }
    va_end(ap);
}

/* ---------------- Multiboot2 minimal parsing ----------------
   We don't require full spec here; we look for:
   - memory map tag (type 6)
   - boot modules tag (type 3)
   GRUB passes the multiboot2 info pointer in RDI (per the multiboot2 spec) if using standard entry.
   We'll accept info pointer argument: see kernel entry signature below.
*/

typedef struct {
    uint32_t total_size;
    uint32_t reserved;
} mb2_header_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} mb2_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t mod_start;
    uint32_t mod_end;
    /* char cmdline[] follows */
} mb2_module_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
    /* entries follow */
} mb2_mmap_tag_t;

typedef struct {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
} mb2_mmap_entry_t;

/* store modules */
#define MAX_MODULES 16
static void* module_start[MAX_MODULES];
static void* module_end[MAX_MODULES];
static const char* module_cmd[MAX_MODULES];
static int module_count = 0;

/* print memory map if available */
static void handle_multiboot2(uint64_t info_addr) {
    if (!info_addr) return;
    mb2_header_t *h = (mb2_header_t*)info_addr;
    uint8_t *p = (uint8_t*)info_addr + sizeof(mb2_header_t);
    uint8_t *end = (uint8_t*)info_addr + h->total_size;
    while (p < end) {
        mb2_tag_t *tag = (mb2_tag_t*)p;
        if (tag->type == 0) break; /* end */
        if (tag->type == 3) { /* modules */
            mb2_module_t *m = (mb2_module_t*)tag;
            if (module_count < MAX_MODULES) {
                module_start[module_count] = (void*)(uintptr_t)m->mod_start;
                module_end[module_count] = (void*)(uintptr_t)m->mod_end;
                module_cmd[module_count] = (const char*)( (uintptr_t)m + sizeof(mb2_module_t) );
                module_count++;
            }
        } else if (tag->type == 6) { /* memory map */
            mb2_mmap_tag_t *mm = (mb2_mmap_tag_t*)tag;
            uint8_t *ent = p + sizeof(mb2_mmap_tag_t);
            uint8_t *limit = p + tag->size;
            kprintf("Memory map entries:\n");
            while (ent + mm->entry_size <= limit) {
                mb2_mmap_entry_t *e = (mb2_mmap_entry_t*)ent;
                kprintf(" addr: %p len: %p type: %d\n", (void*)(uintptr_t)e->addr, (void*)(uintptr_t)e->len, e->type);
                ent += mm->entry_size;
            }
        }
        /* align to 8 */
        uint32_t sz = tag->size;
        p += ( (sz + 7) & ~7 );
    }
}

/* ---------------- Very small PS/2 keyboard reader (polling) ----------------
   We implement a small scancode->char map for common keys (letters, numbers, space, backspace, enter, arrows not implemented).
*/

static unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* shell buffers and history */
#define SHELL_LINE_MAX 256
#define SHELL_HISTORY 8
static char hist[SHELL_HISTORY][SHELL_LINE_MAX];
static int hist_head = 0;
static int hist_count = 0;

static void hist_push(const char *s) {
    for (int i = 0; i < SHELL_LINE_MAX-1 && s[i]; ++i) hist[hist_head][i] = s[i];
    hist[hist_head][SHELL_LINE_MAX-1] = 0;
    hist_head = (hist_head + 1) % SHELL_HISTORY;
    if (hist_count < SHELL_HISTORY) ++hist_count;
}

static void shell_prompt(void) {
    vga_puts("abanta> ");
}

/* execute simple commands */
static void shell_execute(char *line) {
    if (!line) return;
    /* trim leading spaces */
    while (*line == ' ') ++line;
    if (*line == 0) return;
    if (strncmp(line, "help", 4) == 0) {
        vga_puts("Commands:\n");
        vga_puts(" help       - show this\n");
        vga_puts(" clear      - clear screen\n");
        vga_puts(" echo ...   - echo text\n");
        vga_puts(" mem        - show multiboot2 memory map (if provided)\n");
        vga_puts(" modules    - list multiboot modules\n");
        vga_puts(" run <idx>  - jump to module entry (if executable)\n");
        return;
    }
    if (strncmp(line, "clear", 5) == 0) { vga_clear(); return; }
    if (strncmp(line, "echo ", 5) == 0) { vga_puts(line + 5); vga_putch('\n'); return; }
    if (strncmp(line, "mem", 3) == 0) { /* handled at boot */ vga_puts("(use boot logs)\n"); return; }
    if (strncmp(line, "modules", 7) == 0) {
        kprintf("Modules: %d\n", module_count);
        for (int i = 0; i < module_count; ++i) {
            kprintf(" [%d] %s @ %p-%p\n", i, module_cmd[i], module_start[i], module_end[i]);
        }
        return;
    }
    if (strncmp(line, "run ", 4) == 0) {
        int idx = atoi(line + 4);
        if (idx < 0 || idx >= module_count) { vga_puts("bad index\n"); return; }
        void *start = module_start[idx];
        /* Very simple attempt: if module looks like an ELF64 and has an entry, call it.
           This is very advanced; calling random module may crash. Use for controlled tests only.
        */
        /* Check ELF magic */
        unsigned char *p = (unsigned char*)start;
        if (p[0]==0x7f && p[1]=='E' && p[2]=='L' && p[3]=='F') {
            /* parse ELF64 entrypoint at e_entry offset 0x18 */
            uint64_t e_entry = *(uint64_t*)( (uintptr_t)start + 0x18 );
            void (*entry)(void) = (void(*)(void))( (uintptr_t)start + (uintptr_t)e_entry );
            kprintf("Jumping to module entry %p\n", entry);
            entry();
            /* if returns, continue */
            kprintf("module returned\n");
            return;
        } else {
            vga_puts("module not ELF64 or unsupported\n");
        }
        return;
    }
    vga_puts("Unknown command\n");
}

/* tiny atoi */
static int atoi(const char *s) {
    int v = 0; int neg = 0;
    if (*s == '-') { neg = 1; ++s; }
    while (*s >= '0' && *s <= '9') { v = v*10 + (*s - '0'); ++s; }
    return neg ? -v : v;
}

/* kernel entry: called by boot64.S (no args), but we attempt to read multiboot2 info pointer from register rdi:
   Many multiboot2 loaders pass the info pointer in rsi/rdx depending; to be robust we accept a NULL info pointer.
   For simplicity, GRUB2 normally passes the multiboot2 info pointer in RDI (when jumping to kernel with AMD64 calling conventions).
   We'll read it using inline asm.
*/
void kernel_main(void) {
    uint64_t mb2_info = 0;
    __asm__ volatile ("mov %%rdi, %0" : "=r"(mb2_info));
    vga_clear();
    kprintf("Abanta x86_64 multiboot2 kernel\n");
    if (mb2_info) {
        kprintf("multiboot2 info: %p\n", (void*)(uintptr_t)mb2_info);
        handle_multiboot2(mb2_info);
    } else {
        kprintf("no multiboot2 info (GRUB may have not passed it)\n");
    }

    /* very small keyboard input loop (polling PS/2). Map a small set of keys (same as earlier skeleton). */
    shell_prompt();

    char linebuf[SHELL_LINE_MAX];
    int pos = 0;
    for (;;) {
        /* wait for key */
        unsigned char status;
        do { status = inb(0x64); } while (!(status & 1));
        unsigned char sc = inb(0x60);
        char ch = 0;
        switch (sc) {
            case 0x1C: ch = '\n'; break;
            case 0x0E: ch = '\b'; break;
            case 0x39: ch = ' '; break;
            /* numbers */
            case 0x02: ch='1'; break; case 0x03: ch='2'; break; case 0x04: ch='3'; break;
            case 0x05: ch='4'; break; case 0x06: ch='5'; break; case 0x07: ch='6'; break;
            case 0x08: ch='7'; break; case 0x09: ch='8'; break; case 0x0A: ch='9'; break;
            case 0x0B: ch='0'; break;
            /* letters q-p a-l z-m */
            case 0x10: ch='q'; break; case 0x11: ch='w'; break; case 0x12: ch='e'; break;
            case 0x13: ch='r'; break; case 0x14: ch='t'; break; case 0x15: ch='y'; break;
            case 0x16: ch='u'; break; case 0x17: ch='i'; break; case 0x18: ch='o'; break;
            case 0x19: ch='p'; break;
            case 0x1E: ch='a'; break; case 0x1F: ch='s'; break; case 0x20: ch='d'; break;
            case 0x21: ch='f'; break; case 0x22: ch='g'; break; case 0x23: ch='h'; break;
            case 0x24: ch='j'; break; case 0x25: ch='k'; break; case 0x26: ch='l'; break;
            case 0x2C: ch='z'; break; case 0x2D: ch='x'; break; case 0x2E: ch='c'; break;
            case 0x2F: ch='v'; break; case 0x30: ch='b'; break; case 0x31: ch='n'; break;
            case 0x32: ch='m'; break;
            default: ch = 0; break;
        }
        if (!ch) continue;
        if (ch == '\b') {
            if (pos > 0) { pos--; vga_putch('\b'); vga_putch(' '); vga_putch('\b'); }
            continue;
        }
        if (ch == '\n') {
            vga_putch('\n');
            linebuf[pos] = 0;
            if (pos > 0) { hist_push(linebuf); }
            shell_execute(linebuf);
            pos = 0;
            shell_prompt();
            continue;
        }
        if (pos + 1 < SHELL_LINE_MAX) {
            linebuf[pos++] = ch;
            vga_putch(ch);
        } else {
            /* beep or ignore */
        }
    }
}

/* simple strncmp, strncmp used above */
static int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i=0;i<n;++i) {
        if (a[i] != b[i]) return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
        if (a[i] == 0) return 0;
    }
    return 0;
}

static int strlen(const char *s) {
    int i=0; while(s[i]) ++i; return i;
}
