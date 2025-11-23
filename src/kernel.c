/* src/kernel.c
 *
 * Minimal 64-bit kernel with a tiny VGA text-mode shell prompt "abanta>".
 * Builds freestanding: compiled with -ffreestanding, linked with linker.ld.
 *
 * keyboard: polls PS/2 controller port 0x60, converts scancode set 1 (simple)
 * VGA: direct writes to 0xB8000 text buffer (80x25).
 */

typedef unsigned long uint64_t;
typedef unsigned int  uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef unsigned long size_t;

/* VGA text-mode */
static volatile uint16_t *VGA = (uint16_t*)0xB8000;
static int term_row = 0;
static int term_col = 0;
static const int TERM_COLS = 80;
static const int TERM_ROWS = 25;

/* Basic colors */
enum {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_WHITE = 15,
};

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Put a character with attribute to VGA at current cursor */
static void vga_putch_attr(char c, uint8_t attr) {
    if (c == '\n') {
        term_col = 0;
        term_row++;
        if (term_row >= TERM_ROWS) term_row = TERM_ROWS - 1;
        return;
    }
    VGA[term_row * TERM_COLS + term_col] = ((uint16_t)attr << 8) | (uint8_t)c;
    term_col++;
    if (term_col >= TERM_COLS) {
        term_col = 0;
        term_row++;
        if (term_row >= TERM_ROWS) term_row = TERM_ROWS - 1;
    }
}

/* Write NUL-terminated string */
static void vga_puts(const char *s) {
    while (*s) vga_putch_attr(*s++, (VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK);
}

/* Clear screen */
static void vga_clear(void) {
    for (int r=0;r<TERM_ROWS;r++) {
        for (int c=0;c<TERM_COLS;c++) {
            VGA[r*TERM_COLS + c] = ((uint16_t)0x07 << 8) | ' ';
        }
    }
    term_row = 0;
    term_col = 0;
}

/* Simple scancode -> ASCII (partial, lowercase letters, digits, space, backspace, enter) */
static const char scancode_map[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', /* 0x0f */
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',  /* 0x1e */
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',   /* 0x2b */
    0,'\\','z','x','c','v','b','n','m',',','.','/', 0, /* 0x39 */
    ' ', /* 0x39 = space */
    /* rest are zeros */
};

/* Read PS/2 scancode (blocking) */
static unsigned char keyboard_read_scancode(void) {
    unsigned char sc;
    /* Poll until data is available (status 0x60) */
    while (1) {
        unsigned char status = inb(0x64);
        if (status & 1) break;
    }
    sc = inb(0x60);
    return sc;
}

/* Very small input routine that returns printable characters (no modifiers) */
static char read_char_from_keyboard(void) {
    unsigned char sc = keyboard_read_scancode();
    /* ignore key release codes (scancode with high bit set) */
    if (sc & 0x80) return 0;
    if (sc < 128) {
        char c = scancode_map[sc];
        return c;
    }
    return 0;
}

/* print prompt "abanta>" */
static void print_prompt(void) {
    vga_puts("abanta> ");
}

/* kernel_main — called from boot.S */
void kernel_main(void) {
    vga_clear();
    vga_puts("Abanta kernel booted.\n");
    print_prompt();

    /* input buffer */
    char buf[128];
    int idx = 0;

    while (1) {
        char c = read_char_from_keyboard();
        if (!c) continue;

        if (c == '\b') { /* backspace */
            if (idx > 0) {
                idx--;
                /* move cursor back and overwrite */
                if (term_col == 0) {
                    if (term_row > 0) { term_row--; term_col = TERM_COLS - 1; }
                } else term_col--;
                VGA[term_row * TERM_COLS + term_col] = ((uint16_t)0x07 << 8) | ' ';
            }
            continue;
        } else if (c == '\n') {
            vga_putch_attr('\n', (VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK);
            buf[idx] = '\0';
            /* simple built-in commands */
            if (idx == 0) {
                /* empty line just print prompt */
                print_prompt();
            } else {
                /* echo the entered line on next line */
                vga_puts("you typed: ");
                vga_puts(buf);
                vga_putch_attr('\n', (VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK);
                print_prompt();
            }
            idx = 0;
            continue;
        } else if (c == 0) {
            continue;
        } else {
            /* printable */
            if (idx < (int)(sizeof(buf)-1)) {
                buf[idx++] = c;
                vga_putch_attr(c, (VGA_COLOR_WHITE << 4) | VGA_COLOR_BLACK);
            }
        }
    }
}
