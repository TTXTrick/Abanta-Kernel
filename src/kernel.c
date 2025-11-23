/* Minimal 64-bit freestanding kernel skeleton.
   Prints an "abanta>" prompt using VGA text mode (0xB8000).
   The kernel entrypoint is kernel_main called from boot.S
*/

#include <stdint.h>
#include <stddef.h>

/* VGA text buffer */
static volatile uint16_t *VGA = (uint16_t*)0xB8000;
static const int VGA_WIDTH = 80;
static const int VGA_HEIGHT = 25;

/* cursor state */
static size_t cursor_row = 0;
static size_t cursor_col = 0;

/* write a single character with attribute */
static void vga_putc(char c) {
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else {
        const uint8_t attr = 0x0F; /* white on black */
        VGA[cursor_row * VGA_WIDTH + cursor_col] = ((uint16_t)attr << 8) | (uint8_t)c;
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    if (cursor_row >= VGA_HEIGHT) {
        /* simple scroll: move everything up one line */
        for (size_t r = 1; r < VGA_HEIGHT; ++r) {
            for (size_t c = 0; c < VGA_WIDTH; ++c) {
                VGA[(r - 1) * VGA_WIDTH + c] = VGA[r * VGA_WIDTH + c];
            }
        }
        /* clear last line */
        for (size_t c = 0; c < VGA_WIDTH; ++c) VGA[(VGA_HEIGHT - 1) * VGA_WIDTH + c] = (0x0F << 8) | ' ';
        cursor_row = VGA_HEIGHT - 1;
    }
}

/* write a null-terminated string */
static void vga_puts(const char *s) {
    for (size_t i = 0; s[i]; ++i) vga_putc(s[i]);
}

/* write formatted small integer (decimal). No printf dependency. */
static void vga_putdec(unsigned long v) {
    char buf[32];
    int pos = 0;
    if (v == 0) { vga_putc('0'); return; }
    while (v) {
        buf[pos++] = '0' + (v % 10);
        v /= 10;
    }
    for (int i = pos - 1; i >= 0; --i) vga_putc(buf[i]);
}

/* Kernel entry: called from boot.S _start */
void kernel_main(void) {
    /* Clear screen */
    for (size_t r = 0; r < VGA_HEIGHT; ++r)
        for (size_t c = 0; c < VGA_WIDTH; ++c)
            VGA[r * VGA_WIDTH + c] = (0x07 << 8) | ' ';

    /* Print banner */
    vga_puts("Abanta kernel skeleton\n");
    vga_puts("----------------------\n\n");

    /* Print abanta> prompt */
    vga_puts("abanta> ");

    /* very simple loop (no keyboard input implemented in this skeleton) */
    for (;;) {
        /* spin */
        asm volatile("hlt");
    }
}
