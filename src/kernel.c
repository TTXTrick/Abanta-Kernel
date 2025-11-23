/* src/kernel.c
   Minimal 64-bit freestanding kernel skeleton with tiny shell prompt "abanta>"
   This is NOT a full OS — it's a simple kernel + input loop for experimentation.
*/

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

/* VGA text-mode buffer */
static volatile uint16_t *VGA = (uint16_t*)0xB8000;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;

/* Cursor position */
static int cursor_x = 0;
static int cursor_y = 0;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Simple putchar that writes to VGA text buffer. color=0x07 default */
static void putchar_vga(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else {
        VGA[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)((0x07 << 8) | c);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    if (cursor_y >= VGA_HEIGHT) {
        /* scroll up 1 line */
        for (int y = 1; y < VGA_HEIGHT; ++y) {
            for (int x = 0; x < VGA_WIDTH; ++x) {
                VGA[(y-1)*VGA_WIDTH + x] = VGA[y*VGA_WIDTH + x];
            }
        }
        /* clear last line */
        for (int x = 0; x < VGA_WIDTH; ++x) VGA[(VGA_HEIGHT-1)*VGA_WIDTH + x] = (uint16_t)((0x07 << 8) | ' ');
        cursor_y = VGA_HEIGHT - 1;
    }
}

/* Print simple zero-terminated string */
static void puts_vga(const char *s) {
    while (*s) putchar_vga(*s++);
}

/* Print formatted numbers (very small subset) */
static void putdec(uint64_t v) {
    char buf[32];
    int i = 0;
    if (v == 0) { putchar_vga('0'); return; }
    while (v) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    for (int j = i-1; j >= 0; --j) putchar_vga(buf[j]);
}

/* Simple get scancode non-blocking: return 0 if none */
static uint8_t kbd_read_scancode_nonblock(void) {
    /* Port 0x64 status, bit 0 = output buffer full */
    uint8_t status = inb(0x64);
    if ((status & 0x01) == 0) return 0;
    return inb(0x60);
}

/* Scancode -> ASCII for simple keys (set 0, not handling shift) */
static char scancode_to_ascii(uint8_t sc) {
    static const char map[128] = {
        0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b', /* Backspace */
        '\t', /* Tab */
        'q','w','e','r','t','y','u','i','o','p','[',']','\n', /* Enter */
        0, /* Ctrl */
        'a','s','d','f','g','h','j','k','l',';','\'','`',
        0, /* Left shift */
        '\\','z','x','c','v','b','n','m',',','.','/',
        0, /* Right shift */
        '*',
        0, /* Alt */
        ' ', /* Space */
    };
    if (sc < sizeof(map)) return map[sc];
    return 0;
}

/* Very tiny input line buffer */
#define LINE_MAX 256
static char linebuf[LINE_MAX];
static int linelen = 0;

/* Print shell prompt */
static void prompt(void) {
    puts_vga("abanta> ");
}

/* Kernel entrypoint called from assembly */
void kernel_main(void) {
    /* Clear screen */
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; ++i) VGA[i] = (uint16_t)((0x07 << 8) | ' ');
    cursor_x = 0; cursor_y = 0;

    puts_vga("Abanta kernel (multiboot2) - minimal shell\n");
    puts_vga("Type and press Enter. Ctrl-C not implemented.\n\n");
    prompt();

    while (1) {
        uint8_t sc = kbd_read_scancode_nonblock();
        if (!sc) {
            /* no input: tiny pause */
            for (volatile int i = 0; i < 10000; ++i) __asm__ volatile("pause");
            continue;
        }

        char c = scancode_to_ascii(sc);
        if (!c) continue;

        if (c == '\b') {
            if (linelen > 0) {
                linelen--;
                /* backspace on VGA */
                if (cursor_x == 0) {
                    if (cursor_y > 0) { cursor_y--; cursor_x = VGA_WIDTH - 1; }
                } else cursor_x--;
                VGA[cursor_y * VGA_WIDTH + cursor_x] = (uint16_t)((0x07 << 8) | ' ');
            }
        } else if (c == '\n') {
            putchar_vga('\n');
            linebuf[linelen] = 0;
            /* simple builtin: "echo" */
            if (linelen > 0) {
                if (linebuf[0] == 'e' && linebuf[1] == 'c' && linebuf[2] == 'h' && linebuf[3] == 'o' && (linelen==4 || linebuf[4]==' ')) {
                    int start = 5;
                    if (linelen == 4) start = 4;
                    for (int i = start; i < linelen; ++i) putchar_vga(linebuf[i]);
                    putchar_vga('\n');
                } else if (linelen == 0) {
                    /* nothing */
                } else if (linelen == 4 && linebuf[0]=='h' && linebuf[1]=='e' && linebuf[2]=='l' && linebuf[3]=='p') {
                    puts_vga("Commands: help, echo <text>, mem\n");
                } else if (linelen == 3 && linebuf[0]=='m' && linebuf[1]=='e' && linebuf[2]=='m') {
                    puts_vga("Memory info not available in this tiny kernel.\n");
                } else {
                    puts_vga("Unknown command\n");
                }
            }
            linelen = 0;
            prompt();
        } else {
            if (linelen + 1 < LINE_MAX) {
                linebuf[linelen++] = c;
                putchar_vga(c);
            }
        }
    }
}

/* Provide a weak _start alias used by linker entry */
void _start(void) __attribute__((alias("kernel_main")));
