/* Minimal freestanding x86_64 kernel for Abanta
 * - builds as ELF64
 * - simple VGA writer (text mode)
 * - PS/2 keyboard poll to implement a tiny shell "abanta>"
 *
 * This file is freestanding (no libc). Keep it small and clear.
 */

#include <stdint.h>
#include <stddef.h>

/* I/O port helpers */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* VGA text-mode */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
static uint16_t *vga = (uint16_t*)0xB8000;
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

static void vga_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else {
        vga[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, 0x07);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    if (cursor_y >= VGA_HEIGHT) {
        /* simple scroll up */
        for (int y = 1; y < VGA_HEIGHT; ++y) {
            for (int x = 0; x < VGA_WIDTH; ++x) {
                vga[(y-1)*VGA_WIDTH + x] = vga[y*VGA_WIDTH + x];
            }
        }
        /* clear last line */
        for (int x = 0; x < VGA_WIDTH; ++x) vga[(VGA_HEIGHT-1)*VGA_WIDTH + x] = vga_entry(' ', 0x07);
        cursor_y = VGA_HEIGHT - 1;
    }
}

static void vga_puts(const char *s) {
    while (*s) vga_putchar(*s++);
}

static void vga_printf(const char *s) {
    vga_puts(s);
}

/* PS/2 keyboard basic scancode -> ASCII (set 1, unshifted) */
static const char scancode_map[128] = {
  0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
 '\t', 'q','w','e','r','t','y','u','i','o','p','[',']','\n',
  0, /* control */
 'a','s','d','f','g','h','j','k','l',';','\'','`', 0, '\\',
 'z','x','c','v','b','n','m',',','.','/', 0, /* right shift */
 '*', 0, /* alt */
 ' ', /* space */
  0, /* caps lock */
  /* rest are 0 */
};

static char kb_getchar(void) {
    /* Polling PS/2 status */
    /* Wait for output buffer full (port 0x64 bit 0) */
    while (!(inb(0x64) & 1)) {
        /* spin */
    }
    uint8_t code = inb(0x60);
    if (code & 0x80) { /* key release — ignore */
        return 0;
    }
    uint8_t idx = code & 0x7f;
    if (idx < sizeof(scancode_map)) return scancode_map[idx];
    return 0;
}

/* Simple line editor for shell */
#define SHELL_BUF 256

static void shell_prompt(void) {
    vga_puts("abanta> ");
}

void kernel_main(void) {
    /* Clear screen */
    for (int y=0; y<VGA_HEIGHT; ++y) for (int x=0; x<VGA_WIDTH; ++x) vga[(y*VGA_WIDTH)+x] = vga_entry(' ', 0x07);
    cursor_x = 0; cursor_y = 0;

    vga_printf("Abanta kernel (demo) — type 'help' then Enter\n\n");
    shell_prompt();

    char line[SHELL_BUF];
    size_t len = 0;

    while (1) {
        char ch = kb_getchar();
        if (!ch) continue;
        if (ch == '\r' || ch == '\n') {
            /* newline */
            vga_putchar('\n');
            line[len] = '\0';
            if (len == 0) {
                shell_prompt();
                continue;
            }
            /* simple commands */
            if (len == 4 && line[0]=='h' && line[1]=='e' && line[2]=='l' && line[3]=='p') {
                vga_puts("Commands:\n");
                vga_puts("  help   - show this message\n");
                vga_puts("  echo   - echo following text (e.g. echo hello)\n");
                vga_puts("  clear  - clear screen\n");
                vga_puts("  halt   - halt CPU\n");
            } else if (len >= 5 && line[0]=='e' && line[1]=='c' && line[2]=='h' && line[3]=='o' && line[4]==' ') {
                vga_puts(line + 5);
                vga_putchar('\n');
            } else if (len==5 && line[0]=='c' && line[1]=='l' && line[2]=='e' && line[3]=='a' && line[4]=='r') {
                /* clear */
                for (int y=0; y<VGA_HEIGHT; ++y) for (int x=0; x<VGA_WIDTH; ++x) vga[(y*VGA_WIDTH)+x] = vga_entry(' ', 0x07);
                cursor_x = 0; cursor_y = 0;
            } else if (len==4 && line[0]=='h' && line[1]=='a' && line[2]=='l' && line[3]=='t') {
                vga_puts("Halting...\n");
                for (;;) { __asm__ volatile ("hlt"); }
            } else {
                vga_puts("Unknown command: ");
                vga_puts(line);
                vga_putchar('\n');
            }
            len = 0;
            shell_prompt();
            continue;
        }
        if (ch == '\b' || ch == 127) { /* backspace */
            if (len > 0) {
                /* move cursor back and erase */
                if (cursor_x == 0) { if (cursor_y>0) { cursor_y--; cursor_x = VGA_WIDTH - 1; } }
                else cursor_x--;
                vga[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', 0x07);
                len--;
            }
            continue;
        }
        /* printable */
        if (len + 1 < SHELL_BUF && ch >= 32 && ch < 127) {
            line[len++] = ch;
            vga_putchar(ch);
        }
    }
}

/* Provide the entry symbol for the linker script */
void kernel_entry(void);
void kernel_entry(void) {
    kernel_main();
    for (;;) __asm__ volatile ("hlt");
}
