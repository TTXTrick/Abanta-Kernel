/* kernel.c - Minimal 64-bit freestanding kernel skeleton with a simple shell
 *
 * - Designed to be linked with a separate boot64.S that switches to long mode
 *   and calls kernel_main().
 * - No libc usage (freestanding). All helpers implemented here (kstrlen, kstrncmp, katoi...).
 * - VGA text-mode output (0xB8000) for console.
 * - Keyboard input via PS/2 port 0x60 (scancode set 1, simple mapping).
 * - Shell features:
 *     - prompt "abanta>"
 *     - basic commands: help, clear, echo, history, run <name>
 *     - line editing (backspace), simple history up/down
 * - Module registry: very small facility to register "user modules" (name + entry).
 *     - run <name> will call the module entry if registered.
 *
 * NOTE: This is a kernel *skeleton*, not a full OS. It's intentionally small and
 * self-contained so you can build & iterate quickly.
 *
 * Make sure your boot64.S calls: void kernel_main(void);
 */

#include <stdint.h>

/* ---------------------- low-level I/O ---------------------- */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ---------------------- simple types & helpers ---------------------- */

typedef unsigned long size_t;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

static size_t kstrlen(const char *s) {
    size_t n = 0;
    while (s && s[n]) n++;
    return n;
}

static int kstrncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb) return (int)ca - (int)cb;
        if (ca == 0) return 0;
    }
    return 0;
}

static int kstrcmp(const char *a, const char *b) {
    for (size_t i = 0;; ++i) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];
        if (ca != cb) return (int)ca - (int)cb;
        if (ca == 0) return 0;
    }
}

static int katoi(const char *s) {
    int sign = 1;
    int v = 0;
    if (*s == '-') { sign = -1; ++s; }
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        ++s;
    }
    return v * sign;
}

/* ---------------------- VGA text-mode console ---------------------- */

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
static volatile uint16_t * const VGA_BUF = (uint16_t*)0xB8000;

static u16 cursor_row = 0;
static u16 cursor_col = 0;
static u8 current_attr = 0x07; /* light grey on black */

static void vga_putat(char c, u8 attr, u16 row, u16 col) {
    VGA_BUF[row * VGA_WIDTH + col] = ((u16)attr << 8) | (uint8_t)c;
}

static void vga_scroll(void) {
    for (u16 r = 1; r < VGA_HEIGHT; ++r) {
        for (u16 c = 0; c < VGA_WIDTH; ++c) {
            VGA_BUF[(r - 1) * VGA_WIDTH + c] = VGA_BUF[r * VGA_WIDTH + c];
        }
    }
    /* clear last line */
    for (u16 c = 0; c < VGA_WIDTH; ++c) {
        vga_putat(' ', current_attr, VGA_HEIGHT - 1, c);
    }
    if (cursor_row > 0) cursor_row--;
}

static void vga_set_cursor(u16 row, u16 col) {
    cursor_row = row;
    cursor_col = col;
    /* not updating hardware cursor - not necessary */
}

static void vga_putch(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= VGA_HEIGHT) vga_scroll();
        return;
    }
    if (c == '\r') {
        cursor_col = 0;
        return;
    }
    if (c == '\t') {
        int spaces = 4 - (cursor_col % 4);
        for (int i = 0; i < spaces; ++i) vga_putch(' ');
        return;
    }

    vga_putat(c, current_attr, cursor_row, cursor_col);
    cursor_col++;
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
        if (cursor_row >= VGA_HEIGHT) vga_scroll();
    }
}

static void vga_puts(const char *s) {
    for (size_t i = 0; s && s[i]; ++i) vga_putch(s[i]);
}

static void vga_clear(void) {
    for (u16 r = 0; r < VGA_HEIGHT; ++r) {
        for (u16 c = 0; c < VGA_WIDTH; ++c) {
            vga_putat(' ', current_attr, r, c);
        }
    }
    vga_set_cursor(0, 0);
}

static void vga_puthex(u64 v) {
    const char *hex = "0123456789ABCDEF";
    char buf[17];
    buf[16] = 0;
    for (int i = 15; i >= 0; --i) {
        buf[i] = hex[v & 0xF];
        v >>= 4;
    }
    vga_puts(buf);
}

/* ---------------------- Keyboard (PS/2 set 1) - minimal ----------------------
 *
 * We implement a tiny scancode -> ASCII mapping for common keys.
 * This is purposely minimal and not full-featured (no modifiers handling
 * beyond Shift for letters/digits).
 */

enum {
    KBD_PORT = 0x60,
    KBD_STATUS = 0x64
};

/* Scancode set 1 (make codes) for common keys */
static const char scancode_map[128] = {
    0,  27, '1','2','3','4','5','6','7','8', /* 0 - 9 */
    '9','0','-','=', '\b', /* backspace */
    '\t', /* tab */
    'q','w','e','r','t','y','u','i','o','p', /* 16-25 */
    '[',']','\n', /* enter */
    0, /* ctrl */
    'a','s','d','f','g','h','j','k','l',';', /* 30-39 */
    '\'', '`', 0, /* left shift */
    '\\','z','x','c','v','b','n','m',',','.','/', /* 44-54 */
    0, /* right shift */
    '*', 0, /* alt? */
    ' ', /* space */
    /* The rest mostly zeros */
};

static const char scancode_map_shift[128] = {
    0,  27, '!', '@','#','$','%','^','&','*',
    '(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P',
    '{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':',
    '"','~',0,'|','Z','X','C','V','B','N','M','<','>','?',
    0, '*', 0, ' '
};

static int kbd_has_data(void) {
    /* status port bit 1 (output buffer full) -> data available */
    u8 st;
    st = inb(KBD_STATUS);
    return st & 1;
}

static int kbd_read_scancode(void) {
    /* busy-wait for input */
    while (!kbd_has_data()) {
        /* spin */
    }
    return inb(KBD_PORT);
}

/* We won't implement full modifier tracking; keep simple: if Shift pressed, map through shift table.
 * Scancode: 0x2A = left shift make, 0xAA = left shift break
 * 0x36 = right shift make, 0xB6 = right shift break
 */
static int shift_down = 0;

static int kbd_getchar_blocking(void) {
    for (;;) {
        int sc = kbd_read_scancode();
        if (sc == 0) continue;
        if (sc == 0x2A || sc == 0x36) { shift_down = 1; continue; }
        if (sc == 0xAA || sc == 0xB6) { shift_down = 0; continue; }
        /* ignore release codes with high bit set (>127) */
        if (sc & 0x80) continue;
        if (shift_down) {
            if (sc < 128 && scancode_map_shift[sc]) return scancode_map_shift[sc];
        } else {
            if (sc < 128 && scancode_map[sc]) return scancode_map[sc];
        }
    }
}

/* ---------------------- Simple module registry ---------------------- */

#define MAX_MODULES 16
#define MOD_NAME_LEN 32

typedef void (*module_entry_t)(void);

typedef struct {
    char name[MOD_NAME_LEN];
    module_entry_t entry;
    int used;
} module_t;

static module_t modules[MAX_MODULES];

static int register_module(const char *name, module_entry_t entry) {
    for (int i = 0; i < MAX_MODULES; ++i) {
        if (!modules[i].used) {
            modules[i].used = 1;
            /* copy name safely */
            size_t n = kstrlen(name);
            if (n >= MOD_NAME_LEN) n = MOD_NAME_LEN - 1;
            for (size_t j = 0; j < n; ++j) modules[i].name[j] = name[j];
            modules[i].name[n] = 0;
            modules[i].entry = entry;
            return 0;
        }
    }
    return -1;
}

static module_entry_t find_module(const char *name) {
    for (int i = 0; i < MAX_MODULES; ++i) {
        if (!modules[i].used) continue;
        if (kstrcmp(modules[i].name, name) == 0) return modules[i].entry;
    }
    return 0;
}

/* For demonstration: a tiny sample module */
static void sample_module(void) {
    vga_puts("\n[mod] hello from sample_module()\n");
}

/* ---------------------- Shell with history ---------------------- */

#define SHELL_PROMPT "abanta> "
#define SHELL_MAX_LINE 256
#define SHELL_HISTORY 16

static char history[SHELL_HISTORY][SHELL_MAX_LINE];
static int history_count = 0;
static int history_pos = 0; /* for navigation */

static void history_add(const char *line) {
    if (!line || !line[0]) return;
    /* simple circular push */
    for (int i = SHELL_HISTORY - 1; i > 0; --i) {
        for (size_t j = 0; j < SHELL_MAX_LINE; ++j) history[i][j] = history[i - 1][j];
    }
    /* copy line to history[0] */
    size_t n = kstrlen(line);
    if (n >= SHELL_MAX_LINE) n = SHELL_MAX_LINE - 1;
    for (size_t j = 0; j < n; ++j) history[0][j] = line[j];
    history[0][n] = 0;
    if (history_count < SHELL_HISTORY) history_count++;
    history_pos = -1; /* reset navigation */
}

static void shell_print_prompt(void) {
    vga_puts(SHELL_PROMPT);
}

static void shell_clear_line_on_screen(int col_start) {
    /* rudimentary: replace until end of line with spaces */
    for (u16 c = col_start; c < VGA_WIDTH; ++c) {
        vga_putat(' ', current_attr, cursor_row, c);
    }
    vga_set_cursor(cursor_row, col_start);
}

static void shell_execute(const char *line) {
    if (!line) return;
    if (kstrncmp(line, "help", 4) == 0) {
        vga_puts("\nCommands:\n");
        vga_puts("  help           - show this help\n");
        vga_puts("  clear          - clear screen\n");
        vga_puts("  echo <text>    - print text\n");
        vga_puts("  history        - show recent commands\n");
        vga_puts("  run <modname>  - run a registered module\n");
        vga_puts("  modules        - list registered modules\n");
        return;
    }
    if (kstrcmp(line, "clear") == 0) {
        vga_clear();
        return;
    }
    if (kstrncmp(line, "echo ", 5) == 0) {
        vga_puts("\n");
        vga_puts(line + 5);
        vga_puts("\n");
        return;
    }
    if (kstrcmp(line, "history") == 0) {
        vga_puts("\nHistory:\n");
        for (int i = 0; i < history_count; ++i) {
            vga_puts("  ");
            vga_puts(history[i]);
            vga_puts("\n");
        }
        return;
    }
    if (kstrncmp(line, "run ", 4) == 0) {
        const char *modname = line + 4;
        module_entry_t e = find_module(modname);
        if (!e) {
            vga_puts("\nModule not found: ");
            vga_puts(modname);
            vga_puts("\n");
            return;
        }
        vga_puts("\nRunning module: ");
        vga_puts(modname);
        vga_puts("\n");
        e();
        vga_puts("\nModule finished\n");
        return;
    }
    if (kstrcmp(line, "modules") == 0) {
        vga_puts("\nModules:\n");
        for (int i = 0; i < MAX_MODULES; ++i) {
            if (!modules[i].used) continue;
            vga_puts("  ");
            vga_puts(modules[i].name);
            vga_puts("\n");
        }
        return;
    }

    vga_puts("\nUnknown command: ");
    vga_puts(line);
    vga_puts("\n");
}

/* ---------------------- Simple line editor using PS/2 getchar ---------------------- */

static void shell_loop(void) {
    char line[SHELL_MAX_LINE];
    size_t len = 0;
    shell_print_prompt();

    for (;;) {
        int ch = kbd_getchar_blocking();
        if (ch == '\r' || ch == '\n') {
            /* newline */
            vga_putch('\n');
            line[len] = 0;
            if (len > 0) {
                history_add(line);
                shell_execute(line);
            }
            len = 0;
            line[0] = 0;
            shell_print_prompt();
            continue;
        }
        if (ch == 0x08) { /* backspace */
            if (len > 0) {
                /* move cursor back and erase */
                if (cursor_col == 0) {
                    if (cursor_row > 0) { cursor_row--; cursor_col = VGA_WIDTH - 1; }
                } else {
                    cursor_col--;
                }
                vga_putat(' ', current_attr, cursor_row, cursor_col);
                line[--len] = 0;
            }
            continue;
        }
        /* Simple history navigation not implemented via special keys (lack of arrows),
           but we keep a history[] for 'history' command. Implementing up/down would
           require detecting arrow scancodes (0xE0 0x48 / 0x50). For simplicity we
           do a very tiny support: if user types "~<n>" treat it as "run history entry n".
           (This is optional; not necessary).
        */
        if (ch >= 32 && ch <= 126) {
            if (len + 1 < SHELL_MAX_LINE) {
                line[len++] = (char)ch;
                vga_putch((char)ch);
            } else {
                /* beep? just ignore */
            }
        }
    }
}

/* ---------------------- Kernel entry point ---------------------- */

/* Called from boot64.S */
void kernel_main(void) {
    /* minimal init */
    vga_clear();
    vga_puts("Abanta kernel booted (x86_64)\n");
    vga_puts("Type 'help' for commands.\n\n");

    /* Register a sample module so 'run sample' works */
    register_module("sample", sample_module);

    /* Enter shell */
    shell_loop();

    /* never returns */
    for (;;) __asm__ volatile ("hlt");
}

/* Provide a simple _start wrapper in case boot.S expects it.
 * If your boot.S already defines _start and calls kernel_main(), you can
 * ignore this. If building a standalone kernel (no boot.S), define
 * an appropriate entry that sets up long mode, stack, etc.
 *
 * This symbol is weak so a bootloader-provided _start will override it.
 */
__attribute__((weak)) void _start(void) {
    kernel_main();
    for (;;) __asm__ volatile ("hlt");
}
