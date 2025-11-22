#include "kernel.h"

/* Minimal printing to VGA text buffer and small shell */
static volatile unsigned short *VGA = (unsigned short *)0xB8000;
static int cursor_row = 0, cursor_col = 0;

static void putchar(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else {
        VGA[cursor_row * 80 + cursor_col] = (unsigned short)c | (0x07 << 8);
        cursor_col++;
        if (cursor_col >= 80) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    if (cursor_row >= 25) {
        /* scroll simple: move all up one row */
        for (int r = 1; r < 25; ++r)
            for (int c = 0; c < 80; ++c)
                VGA[(r - 1) * 80 + c] = VGA[r * 80 + c];
        /* clear last row */
        int r = 24;
        for (int c = 0; c < 80; ++c) VGA[r * 80 + c] = ' ' | (0x07 << 8);
        cursor_row = 24;
    }
}

/* write string */
static void puts(const char *s) {
    while (*s) putchar(*s++);
}

/* convert scancode to ascii (very small map for keys 2..13 top row + letters)
   We'll implement a tiny US layout map from set 1 scancodes (PS/2 port 0x60).
   This is NOT comprehensive. It's sufficient to type lowercase letters, backspace and enter.
*/
static char scancode_map[256];
static void init_scancode_map(void) {
    for (int i = 0; i < 256; ++i) scancode_map[i] = 0;
    scancode_map[0x02] = '1';
    scancode_map[0x03] = '2';
    scancode_map[0x04] = '3';
    scancode_map[0x05] = '4';
    scancode_map[0x06] = '5';
    scancode_map[0x07] = '6';
    scancode_map[0x08] = '7';
    scancode_map[0x09] = '8';
    scancode_map[0x0A] = '9';
    scancode_map[0x0B] = '0';
    scancode_map[0x0C] = '-';
    scancode_map[0x0D] = '=';
    scancode_map[0x10] = 'q';
    scancode_map[0x11] = 'w';
    scancode_map[0x12] = 'e';
    scancode_map[0x13] = 'r';
    scancode_map[0x14] = 't';
    scancode_map[0x15] = 'y';
    scancode_map[0x16] = 'u';
    scancode_map[0x17] = 'i';
    scancode_map[0x18] = 'o';
    scancode_map[0x19] = 'p';
    scancode_map[0x1E] = 'a';
    scancode_map[0x1F] = 's';
    scancode_map[0x20] = 'd';
    scancode_map[0x21] = 'f';
    scancode_map[0x22] = 'g';
    scancode_map[0x23] = 'h';
    scancode_map[0x24] = 'j';
    scancode_map[0x25] = 'k';
    scancode_map[0x26] = 'l';
    scancode_map[0x2C] = 'z';
    scancode_map[0x2D] = 'x';
    scancode_map[0x2E] = 'c';
    scancode_map[0x2F] = 'v';
    scancode_map[0x30] = 'b';
    scancode_map[0x31] = 'n';
    scancode_map[0x32] = 'm';
    scancode_map[0x39] = ' '; /* space */
    scancode_map[0x1C] = '\n'; /* Enter */
    scancode_map[0x0E] = '\b'; /* Backspace */
}

/* Read from PS/2 data port 0x60 (polling). Returns scancode (0 if none) */
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Wait for a scancode and translate to ascii, blocking */
static char getch_block(void) {
    while (1) {
        unsigned char s = inb(0x64); /* status port */
        if (s & 1) { /* output buffer full */
            unsigned char sc = inb(0x60);
            /* ignore releases (scancodes >= 0x80) for simplicity */
            if (sc & 0x80) continue;
            char c = scancode_map[sc];
            if (c) return c;
        }
    }
}

/* Tiny shell */
void shell_loop(void) {
    char line[128];
    int len = 0;
    puts("abanta> ");
    while (1) {
        char c = getch_block();
        if (c == '\n') {
            putchar('\n');
            line[len] = 0;
            if (len == 0) {
                puts("abanta> ");
                continue;
            }
            /* handle commands: help, echo, clear, reboot (makes QEMU exit via triple fault) */
            if (len == 4 && line[0]=='h' && line[1]=='e' && line[2]=='l' && line[3]=='p') {
                puts("Commands:\n help\n echo <text>\n clear\n reboot\n");
            } else if (len >= 5 && line[0]=='e' && line[1]=='c' && line[2]=='h' && line[3]=='o' && line[4]==' ') {
                puts(line + 5);
                putchar('\n');
            } else if (len == 5 && line[0]=='c' && line[1]=='l' && line[2]=='e' && line[3]=='a' && line[4]=='r') {
                /* clear screen */
                for (int r = 0; r < 25; ++r)
                    for (int cc = 0; cc < 80; ++cc)
                        VGA[r*80 + cc] = ' ' | (0x07 << 8);
                cursor_row = 0;
                cursor_col = 0;
            } else if (len == 6 && line[0]=='r' && line[1]=='e' && line[2]=='b' && line[3]=='o' && line[4]=='o' && line[5]=='t') {
                /* cause CPU halt loop (QEMU will show it) */
                puts("Rebooting (halt)...\n");
                for (;;) { __asm__ volatile ("hlt"); }
            } else {
                puts("Unknown command\n");
            }
            /* new prompt */
            len = 0;
            puts("abanta> ");
        } else if (c == '\b') {
            if (len > 0) {
                len--;
                /* backspace on screen */
                if (cursor_col > 0) {
                    cursor_col--;
                } else {
                    if (cursor_row > 0) {
                        cursor_row--;
                        cursor_col = 79;
                    }
                }
                VGA[cursor_row*80 + cursor_col] = ' ' | (0x07 << 8);
            }
        } else {
            if (len + 1 < (int)sizeof(line)) {
                line[len++] = c;
                putchar(c);
            }
        }
    }
}

/* Kernel entry from boot.S */
void kernel_main(void) {
    /* simple init */
    init_scancode_map();
    puts("Abanta kernel — enter 'abanta>' shell\n");
    shell_loop();
    for (;;) __asm__ volatile("hlt");
}
