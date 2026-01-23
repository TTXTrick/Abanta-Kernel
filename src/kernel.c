/* src/kernel.c
 *
 * Minimal freestanding 64-bit kernel with a small interactive shell.
 * - VGA text output
 * - PS/2 keyboard polling (scancode set 1, basic extended-key support)
 * - Command history (Up/Down arrows)
 * - Builtins: help, clear, echo, history, run <hexaddr>, reboot
 *
 * No libc. Self-contained helper implementations (kputs/kprintf/strings).
 *
 * Entrypoint expected: void kernel_main(void);
 *
 * WARNING: "run <hexaddr>" will jump to arbitrary memory. Use only with
 * code you trust / have loaded at that address (ELF modules, hand-placed code).
 */

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* ---------------- basic types & small helpers ---------------- */

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    asm volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" :: "a"(val), "Nd"(port));
}

/* ---------------- VGA text-mode output ---------------- */

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
static volatile uint16_t *vga = (uint16_t *)0xB8000;
static uint8_t vga_attr = 0x07; /* you have seen too much of this */
static size_t cursor_row = 0, cursor_col = 0;

static void vga_update_cursor(void) {
    /* optional: implement hardware cursor later - we won't use it here */
}

static void vga_scroll_if_needed(void) {
    if (cursor_row < VGA_HEIGHT) return;
    /* scroll up by 1 */
    for (size_t r = 1; r < VGA_HEIGHT; ++r) {
        for (size_t c = 0; c < VGA_WIDTH; ++c) {
            vga[(r-1)*VGA_WIDTH + c] = vga[r*VGA_WIDTH + c];
        }
    }
    /* clear last line */
    size_t last = (VGA_HEIGHT - 1) * VGA_WIDTH;
    for (size_t c = 0; c < VGA_WIDTH; ++c) vga[last + c] = ((uint16_t)vga_attr << 8) | ' ';
    cursor_row = VGA_HEIGHT - 1;
}

static void kputc(char ch) {
    if (ch == '\n') {
        cursor_col = 0;
        cursor_row++;
        vga_scroll_if_needed();
        return;
    }
    if (ch == '\r') { cursor_col = 0; return; }
    if (ch == '\t') {
        int tab = 4 - (cursor_col % 4);
        for (int i = 0; i < tab; ++i) kputc(' ');
        return;
    }
    if (ch == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            size_t off = cursor_row * VGA_WIDTH + cursor_col;
            vga[off] = ((uint16_t)vga_attr << 8) | ' ';
        }
        return;
    }
    size_t off = cursor_row * VGA_WIDTH + cursor_col;
    vga[off] = ((uint16_t)vga_attr << 8) | (uint8_t)ch;
    cursor_col++;
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
        vga_scroll_if_needed();
    }
}

static void kputs(const char *s) {
    for (; *s; ++s) kputc(*s);
}

static void kputhex(unsigned long v) {
    char buf[17];
    buf[16] = '\0';
    for (int i = 15; i >= 0; --i) {
        int nib = v & 0xF;
        buf[i] = (nib < 10) ? ('0' + nib) : ('a' + (nib - 10));
        v >>= 4;
    }
    kputs("0x");
    kputs(buf);
}

static void kputdec(unsigned long v) {
    char buf[24];
    int i = 0;
    if (v == 0) { kputc('0'); return; }
    while (v) { buf[i++] = '0' + (v % 10); v /= 10; }
    for (int j = i-1; j >= 0; --j) kputc(buf[j]);
}

/* minimal printf-like */
static void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    for (const char *p = fmt; *p; ++p) {
        if (*p != '%') { kputc(*p); continue; }
        ++p;
        if (*p == '\0') break;
        switch (*p) {
        case 's': {
            const char *s = va_arg(ap, const char*);
            if (!s) s = "(null)";
            kputs(s);
            break;
        }
        case 'c': {
            int c = va_arg(ap, int);
            kputc((char)c);
            break;
        }
        case 'd':
        case 'u': {
            unsigned long d = va_arg(ap, unsigned long);
            kputdec(d);
            break;
        }
        case 'x':
        case 'p': {
            unsigned long x = va_arg(ap, unsigned long);
            kputhex(x);
            break;
        }
        case '%': kputc('%'); break;
        default:
            kputc('%'); kputc(*p);
        }
    }
    va_end(ap);
}

/* clear screen */
static void kclear(void) {
    size_t total = VGA_WIDTH * VGA_HEIGHT;
    for (size_t i = 0; i < total; ++i) vga[i] = ((uint16_t)vga_attr << 8) | ' ';
    cursor_row = 0; cursor_col = 0;
}

/* ---------------- small string helpers (k-prefixed to avoid libc) ---------------- */

static size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s && *s++) ++n;
    return n;
}
static int kstrncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        unsigned char ca = a[i], cb = b[i];
        if (ca != cb) return (int)ca - (int)cb;
        if (ca == '\0') return 0;
    }
    return 0;
}
static char *kstrcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++));
    return dst;
}
static unsigned long kstrtoul_hex(const char *s) {
    unsigned long v = 0;
    while (*s) {
        char c = *s++;
        int nib;
        if (c >= '0' && c <= '9') nib = c - '0';
        else if (c >= 'a' && c <= 'f') nib = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') nib = c - 'A' + 10;
        else break;
        v = (v << 4) | (unsigned)nib;
    }
    return v;
}

/* ---------------- PS/2 keyboard scancode handling (small) ----------------
   - Polls port 0x60.
   - Supports extended-prefix 0xE0 for arrow keys (Up/Down).
   - Ignores key releases (0x80 bit).
   - Provides ASCII for common keys; no SHIFT handling (so uppercase non-handled).
*/

enum {
    KEY_NONE = 0,
    KEY_UP = 0x100,
    KEY_DOWN = 0x101,
    KEY_LEFT = 0x102,
    KEY_RIGHT = 0x103
};

/* scancode set 1 map (unshifted) for common keys */
static const char scmap[128] = {
/* 0x00..0x0f */  0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
/* 0x10..0x1f */  '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
/* 0x20..0x2f */  'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x',
/* 0x30..0x3f */  'c','v','b','n','m',',','.','/','\n', '*', 0,' ', 0, 0, 0, 0,
/* 0x40..0x4f */  0, 0, 0, 0, 0, 0, 0, '7','8','9','-','4','5','6','+','1',
/* 0x50..0x5f */  '2','3','0','.','\0', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int kb_getkey(void) {
    int extended = 0;
    while (1) {
        uint8_t sc = inb(0x60);
        if (sc == 0xE0) { extended = 1; continue; }
        /* ignore key releases */
        if (sc & 0x80) { extended = 0; continue; }
        if (extended) {
            if (sc == 0x48) return KEY_UP;
            if (sc == 0x50) return KEY_DOWN;
            if (sc == 0x4B) return KEY_LEFT;
            if (sc == 0x4D) return KEY_RIGHT;
            extended = 0;
            /* fallthrough to mapping if desired */
        }
        if (sc < 128 && scmap[sc]) return (int)scmap[sc];
    }
}

/* ---------------- Shell: state, history, line editing ---------------- */

#define SHELL_LINE_MAX 256
#define SHELL_HISTORY 16

static char line_buf[SHELL_LINE_MAX];
static char history[SHELL_HISTORY][SHELL_LINE_MAX];
static int history_count = 0;
static int history_pos = 0; /* for navigation; 0..history_count */

static void push_history(const char *ln) {
    if (!ln || ln[0] == '\0') return;
    /* avoid duplicate consecutive entry */
    if (history_count > 0) {
        if (kstrncmp(history[(history_count-1) % SHELL_HISTORY], ln, SHELL_LINE_MAX) == 0) return;
    }
    if (history_count < SHELL_HISTORY) {
        kstrcpy(history[history_count], ln);
    } else {
        /* rotate left 1 */
        for (int i = 0; i < SHELL_HISTORY-1; ++i) kstrcpy(history[i], history[i+1]);
        kstrcpy(history[SHELL_HISTORY-1], ln);
    }
    history_count++;
}

/* erase current typed characters from screen (backspace them) */
static void erase_prompt_chars(size_t count) {
    for (size_t i = 0; i < count; ++i) {
        kputc('\b'); kputc(' '); kputc('\b');
    }
}

/* print prompt */
static void prompt(void) {
    kputs("abanta> ");
}

/* ---------------- built-in commands ---------------- */

static void cmd_help(void) {
    kputs("Commands:\n");
    kputs("  help            - show this help\n");
    kputs("  clear           - clear the screen\n");
    kputs("  echo ...        - print text\n");
    kputs("  history         - list command history\n");
    kputs("  run <hexaddr>   - jump to code at hex address (use with care)\n");
    kputs("  reboot          - reboot the machine\n");
}

/* keyboard-controller reset (works on many PCs) */
static void do_reboot(void) {
    kputs("Rebooting...\n");
    /* attempt keyboard controller reset */
    outb(0x64, 0xFE);
    for(;;) asm volatile ("hlt");
}

/* shell execute implementation */
static void shell_execute(const char *ln) {
    if (!ln) return;
    if (kstrncmp(ln, "help", 4) == 0) { cmd_help(); return; }
    if (kstrncmp(ln, "clear", 5) == 0) { kclear(); return; }
    if (kstrncmp(ln, "echo ", 5) == 0) { kputs(ln + 5); kputc('\n'); return; }
    if (kstrncmp(ln, "history", 7) == 0) {
        int start = (history_count > SHELL_HISTORY) ? (history_count - SHELL_HISTORY) : 0;
        for (int i = 0; i < history_count && i < SHELL_HISTORY; ++i) {
            int idx = i + start;
            kprintf("%d: %s\n", (unsigned long)idx, history[i]);
        }
        return;
    }
    if (kstrncmp(ln, "run ", 4) == 0) {
        const char *arg = ln + 4;
        unsigned long addr = kstrtoul_hex(arg);
        if (addr == 0) { kputs("Invalid address (hex) or 0\n"); return; }
        kprintf("Jumping to %p ...\n", addr);
        void (*entry)(void) = (void(*)(void))(uintptr_t)addr;
        entry();
        /* If it returns, print something */
        kputs("\nReturned from run()\n");
        return;
    }
    if (kstrncmp(ln, "reboot", 6) == 0) { do_reboot(); return; }

    kputs("Unknown command: ");
    kputs(ln);
    kputc('\n');
}

/* ---------------- main shell input loop ---------------- */

static void shell_loop(void) {
    prompt();
    size_t len = 0;
    history_pos = history_count; /* position for navigation (end) */
    line_buf[0] = '\0';

    while (1) {
        int k = kb_getkey();
        if (k == KEY_UP) {
            /* navigate up: previous entry */
            if (history_count == 0) continue;
            if (history_pos > 0) history_pos--;
            /* erase current line */
            erase_prompt_chars(len);
            /* copy history into buffer */
            const char *h = history[history_pos];
            kstrcpy(line_buf, h);
            len = kstrlen(line_buf);
            kputs(line_buf);
            continue;
        } else if (k == KEY_DOWN) {
            if (history_count == 0) continue;
            if (history_pos < history_count - 1) history_pos++;
            else { history_pos = history_count; /* empty */ }
            erase_prompt_chars(len);
            if (history_pos == history_count) {
                line_buf[0] = '\0'; len = 0;
            } else {
                kstrcpy(line_buf, history[history_pos]);
                len = kstrlen(line_buf);
                kputs(line_buf);
            }
            continue;
        } else if (k == '\n') {
            kputc('\n');
            if (len > 0) {
                push_history(line_buf);
            }
            shell_execute(line_buf);
            /* reset */
            len = 0;
            line_buf[0] = '\0';
            prompt();
        } else if (k == '\b') {
            if (len > 0) {
                kputc('\b'); kputc(' '); kputc('\b');
                len--;
                line_buf[len] = '\0';
            }
        } else if (k >= 32 && k < 127) {
            if (len + 1 < SHELL_LINE_MAX) {
                kputc((char)k);
                line_buf[len++] = (char)k;
                line_buf[len] = '\0';
            } else {
                /* ring bell */
                kputc('\a');
            }
        }
    }
}

/* ---------------- entry point ---------------- */

/* kernel_main should match the label your assembler/bootloader calls.
   If your boot stub passes multiboot params, change signature accordingly.
*/
void kernel_main(void) {
    kclear();
    kputs("Abanta kernel (64-bit) booted.\n");
    kputs("Type 'help' for commands.\n\n");

    shell_loop();

    /* unreachable */
    for (;;) asm volatile ("hlt");
}
